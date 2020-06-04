/*
    fungw - language-agnostic function gateway
    Copyright (C) 2018,2019  Tibor 'Igor2' Palinkas

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    Project page: http://repo.hu/projects/fungw
    Version control: svn://repo.hu/fungw/trunk
*/

#include <stdlib.h>
#include <string.h>
#include <libfungw/fungw.h>
#include <libfungw/fungw_conv.h>

/* Python redefines this blindly */
#undef _POSIX_C_SOURCE
#include <Python.h>

typedef struct {
	PyObject *module, *dict;
	char modname[64];
	PyThreadState *interp;
	fgw_obj_t *obj;
} py_ctx_t;

typedef struct {
	char *name;
	fgw_obj_t *obj;
	py_ctx_t *ctx;
} py_func_t;

static void fgws_python_obj2arg(fgw_arg_t *dst, PyObject *src)
{
	PyTypeObject *type = Py_TYPE(src);

	if (type == &PyUnicode_Type) {
		dst->type = FGW_STR | FGW_DYN;
		dst->val.str = fgw_strdup(PyUnicode_AsUTF8(PyObject_Str(src)));
	}
	else if (type == &PyFloat_Type) {
		dst->type = FGW_DOUBLE;
		dst->val.nat_double = PyFloat_AsDouble(src);
	}
	else if (type == &PyBool_Type) {
		dst->type = FGW_INT;
		dst->val.nat_int = PyBool_Check(src) ? 1 : 0;
	}
	else if (type == &PyLong_Type) {
		dst->type = FGW_LONG;
		dst->val.nat_long = PyLong_AsLong(src);
	}
	else {
		fprintf(stderr, "fgws_python_obj2arg: ignoring unknown type %s\n", type->tp_name);
		dst->type = FGW_PTR;
		dst->val.ptr_void = NULL;
	}
}

PyObject *fgws_python_arg2obj(fgw_ctx_t *fctx, fgw_arg_t *arg)
{
#	define FGW_PY_SET_LONG(lst, val)       return PyLong_FromLong(val);
#	define FGW_PY_SET_LLONG(lst, val)      return PyLong_FromLongLong(val);
#	define FGW_PY_SET_DOUBLE(lst, val)     return PyFloat_FromDouble(val);
#	define FGW_PY_SET_PTR(lst, val)        return PyLong_FromVoidPtr(val);
#	define FGW_PY_SET_NIL(lst, val)        return Py_False;

/* NOTE: have to use PyBytes here, see below at #PYBYTESBUG */
#	define FGW_PY_SET_STR(lst, val)        return PyBytes_FromString(val);

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_PY_SET_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_PY_SET_LLONG);
		ARG_CONV_CASE_DOUBLE(NULL, FGW_PY_SET_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(NULL, FGW_PY_SET_DOUBLE);
		ARG_CONV_CASE_PTR(lst, FGW_PY_SET_PTR);
		ARG_CONV_CASE_STR(NULL, FGW_PY_SET_STR);
		ARG_CONV_CASE_CLASS(lst, FGW_PY_SET_NIL);
		ARG_CONV_CASE_INVALID(lst, FGW_PY_SET_NIL);
	}

	return Py_None;
}

/* API: the script is calling an fgw function */
static PyObject *fgws_python_call_fgw(PyObject *self, PyObject *args)
{
	py_func_t *func_ctx;
	fgw_func_t *func;
	int argc, n;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_error_t err;
	PyObject *py_res;

	func_ctx = PyCapsule_GetPointer(self, NULL);
	func = fgw_func_lookup(func_ctx->obj->parent, func_ctx->name);
	if (func == NULL) {
		fgw_async_error(func_ctx->obj, "fgws_python_call_fgw: function to be called is not found:");
		fgw_async_error(func_ctx->obj, func_ctx->name);
		fgw_async_error(func_ctx->obj, "\n");
		return Py_None;
	}

	argc = PyTuple_Size(args);
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = func_ctx->obj->script_user_call_ctx;


	/* Convert all params to fgw args */
	for (n = 0; n < argc; n++)
		fgws_python_obj2arg(&argv[n+1], PyTuple_GetItem(args, n));

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc+1, argv);

	/* Free the array */
	fgw_argv_free(func_ctx->obj->parent, argc, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return 0;

	py_res = fgws_python_arg2obj(func->obj->parent, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return py_res;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_python_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	py_ctx_t *ctx = obj->script_data;
	py_func_t *func_ctx;
	PyObject *new, *fc;
	PyMethodDef *new_func_define;
	char *new_name = fgw_strdup(name);
	PyMethodDef met[] = {
		{NULL, fgws_python_call_fgw, METH_VARARGS, NULL},
		{NULL, NULL, 0, NULL}
	};

	PyThreadState_Swap(ctx->interp);

	met->ml_name = new_name;
	new_func_define = malloc(sizeof(met));
	memcpy(new_func_define, met, sizeof(met));

	/* save function call context */
	func_ctx = malloc(sizeof(py_func_t));
	func_ctx->name = new_name;
	func_ctx->obj = obj;
	func_ctx->ctx = ctx;
	fc = PyCapsule_New(func_ctx, NULL, NULL);
	new = PyCFunction_New(new_func_define, fc);

	if ((new == NULL) || PyDict_SetItemString(ctx->dict, new_name, new)) {
		fgw_async_error(obj, "fgws_python_reg_func: failed to register function:");
		fgw_async_error(obj, new_name);
		fgw_async_error(obj, "\n");

		if (PyErr_Occurred())
			PyErr_Print();

		free(new_name);
		free(func_ctx);
		PyThreadState_Swap(NULL);
		return;
	}

	Py_DECREF(new);
	PyThreadState_Swap(NULL);
}

/* API: fgw calls a python function */
static fgw_error_t fgws_python_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	py_ctx_t *ctx = obj->script_data;
	PyObject *pfunc, *pargs, *pv, *pret;
	const char *func_name = argv[0].val.func->name;
	int n;
	fgw_error_t ret = FGW_SUCCESS;

	PyThreadState_Swap(ctx->interp);

	pfunc = PyDict_GetItemString(ctx->dict, func_name);

	if ((pfunc == NULL) || !PyCallable_Check(pfunc)) {
		fgw_async_error(obj, "Not a callable python object:");
		fgw_async_error(obj, func_name);
		fgw_async_error(obj, "\n");
		if (PyErr_Occurred())
			PyErr_Print();
		PyThreadState_Swap(NULL);
		return FGW_ERR_NOT_FOUND;
	}

	/* Create the args-list */
	pargs = PyTuple_New(argc-1);

	for (n = 1; n < argc; n++ ) {
		pv = fgws_python_arg2obj(obj->parent, &argv[n]);
		PyTuple_SetItem(pargs, n-1, pv);
	}

	fgws_ucc_save(obj);
	pret = PyObject_CallObject(pfunc, pargs);
	fgws_ucc_restore(obj);

/* #PYBYTESBUG: Py_DECREF(pv) causes a segfault on PyUnicode on uninit-time */
	for(n = 1; n < argc; n++) {
		pv = PyTuple_GetItem(pargs, n-1);
		Py_DECREF(pv);
	}

	if (pret != NULL)
		fgws_python_obj2arg(res, pret);
	else
		res->type = FGW_INVALID;

	Py_DECREF(pargs);
	if (pret != NULL) {
		Py_DECREF(pret);
	}

	if (PyErr_Occurred()) {
		PyErr_Print();
		ret = FGW_ERR_UNKNOWN;
	}

	PyThreadState_Swap(NULL);
	return ret;
}

static int py_global_inited = 0;
static PyThreadState *py_main_interp;
static PyGILState_STATE py_main_state;

/* API: unload the script */
static int fgws_python_unload(fgw_obj_t *obj)
{
	py_ctx_t *ctx = obj->script_data;

	if (ctx->interp != NULL) {
		PyThreadState_Swap(ctx->interp);
		Py_EndInterpreter(ctx->interp);
	}
	PyThreadState_Swap(NULL);
	free(ctx);

	py_global_inited--;
	if (py_global_inited == 0) {
		PyThreadState_Swap(py_main_interp);
		PyGILState_Release(py_main_state);
		Py_Finalize();
	}

	return 0;
}

/* Helper function for the script to register its functions */
static PyObject *fgws_python_freg(PyObject *self, PyObject *args)
{
	py_func_t *func_ctx = PyCapsule_GetPointer(self, NULL);
	fgw_obj_t *obj = func_ctx->obj;
	PyObject *pfunc;
	const char *name;
	int argc;
	fgw_func_t *func;

	argc = PyTuple_Size(args);
	if (argc != 1) {
		fgw_async_error(obj, "fgw_func_reg: wrong number of arguments: need 1\n");
		return 0;
	}

	name = PyUnicode_AsUTF8(PyObject_Str(PyTuple_GetItem(args, 0)));
	if (name == NULL) {
		fgw_async_error(obj, "fgw_func_reg: empty name\n");
		return 0;
	}

	pfunc = PyDict_GetItemString(func_ctx->ctx->dict, name);
	if (pfunc == NULL) {
		fgw_async_error(obj, "fgw_func_reg: function doesn't exist:");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
		if (PyErr_Occurred())
			PyErr_Print();
		return 0;
	}

	func = fgw_func_reg(obj, name, fgws_python_call_script);
	PyThreadState_Swap(func_ctx->ctx->interp); /* side effect: _reg_func(): has set it to NULL (for making sure no active interpreter is left lying around between calls) */
	if (func == NULL) {
		fgw_async_error(obj, "fgw_python_func_reg: failed to register function: ");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
		return 0;
	}

	return Py_True;
}

static int fgws_python_setup_glob(py_ctx_t *ctx)
{
	/* add the python->fgw glue */
	{
		py_func_t *func_ctx;
		PyObject *new, *fc;
		PyMethodDef *new_func_define;
		char *new_name = fgw_strdup("fgw_func_reg");
		PyMethodDef met[] = {
			{"fgw_func_reg", fgws_python_freg, METH_VARARGS, NULL},
			{NULL, NULL, 0, NULL}
		};

		met->ml_name = new_name;
		new_func_define = malloc(sizeof(met));
		memcpy(new_func_define, met, sizeof(met));


		func_ctx = malloc(sizeof(py_func_t));
		func_ctx->name = new_name;
		func_ctx->obj = ctx->obj;
		func_ctx->ctx = ctx;
		fc = PyCapsule_New(func_ctx, NULL, NULL);
		new = PyCFunction_New(new_func_define, fc);

		if ((new == NULL) || PyDict_SetItemString(ctx->dict, new_name, new)) {
			fgw_async_error(ctx->obj, "fgws_python_init: failed to register function: fgw_func_reg\n");

			if (PyErr_Occurred())
				PyErr_Print();

			free(new_name);
			free(func_ctx);
			PyThreadState_Swap(NULL);
			return -1;
		}
		Py_DECREF(new);
	}
	return 0;
}

/* API: init the interpreter so that functions can be registered */
static int fgws_python_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	py_ctx_t *ctx;
	static long py_global_cnt = 0;

	ctx = calloc(sizeof(py_ctx_t), 1);
	if (ctx == NULL) {
		fgw_async_error(obj, "fgws_python_init: failed to allocate context\n");
		return -1;
	}
	obj->script_data = ctx;
	ctx->obj = obj;

	if (py_global_inited == 0) {
		Py_Initialize();
		py_main_interp = PyThreadState_Get(); /* Need to keep this only for GIL uninit */
		py_main_state = PyGILState_Ensure();
	}

	ctx->interp = Py_NewInterpreter();
	PyThreadState_Swap(ctx->interp);

	sprintf(ctx->modname, "_fungw_%ld_", py_global_cnt++);
	{
		PyObject *bimod;
		struct PyModuleDef *python_empty_mod, python_empty_mod_tmp = {
			PyModuleDef_HEAD_INIT,
			"foo",
			"fungw loaded script module",
			-1, /* sizeof */
			NULL, /* methods */
			NULL,
			NULL,
			NULL,
			NULL
		};
		python_empty_mod = malloc(sizeof(struct PyModuleDef));
		memcpy(python_empty_mod, &python_empty_mod_tmp, sizeof(struct PyModuleDef));
		python_empty_mod->m_name = ctx->modname;

		bimod = PyImport_ImportModule("builtins");
		if (bimod == NULL) /* this shouldn't ever fail, but better check */
			return -1;
		ctx->dict = PyDict_New();
		PyDict_SetItemString(ctx->dict, "__builtins__", bimod); /* set up builtins for the new module */
		fgws_python_setup_glob(ctx);
	}
	py_global_inited++;

	PyThreadState_Swap(NULL);
	return 0;
}

/* API: load a script into an object */
static int fgws_python_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	py_ctx_t *ctx = obj->script_data;
	FILE *f;

	PyThreadState_Swap(ctx->interp);

	f = fopen(filename, "r");
	ctx->module = PyRun_File(f, filename, Py_file_input, ctx->dict, ctx->dict);
	fclose(f);

	if (ctx->module == NULL) {
		fgw_async_error(obj, "Failed to load python script:");
		fgw_async_error(obj, filename);
		fgw_async_error(obj, "\n");

		if (PyErr_Occurred())
			PyErr_Print();

		PyThreadState_Swap(NULL);
		return -1;
	}


	PyThreadState_Swap(NULL);
	return 0;
}

static int fgws_python_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".py", ".py3", ".python", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_python_eng = {
	"python",
	fgws_python_call_script,
	fgws_python_init,
	fgws_python_load,
	fgws_python_unload,
	fgws_python_reg_func,
	NULL,
	fgws_python_test_parse
};

int pplg_check_ver_fungw_python(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_python(void)
{
	fgw_eng_reg(&fgw_python_eng);
	return 0;
}

void pplg_uninit_fungw_python(void)
{
	fgw_eng_unreg(fgw_python_eng.name);
}
