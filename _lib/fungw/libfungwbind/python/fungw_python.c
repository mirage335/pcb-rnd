/*
    fungw - language-agnostic function gateway
    Copyright (C) 2018, 2019  Tibor 'Igor2' Palinkas

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
	PyObject *root_module, *root_dict;
	PyObject *module, *dict;
	char modname[64];
} py_ctx_t;

typedef struct {
	char *name;
	fgw_obj_t *obj;
	py_ctx_t *ctx;
} py_func_t;

static PyMethodDef python_empty_method[] = {
	{NULL, NULL, 0, NULL}
};


static void fgws_python_obj2arg(fgw_arg_t *dst, PyObject *src)
{
	PyTypeObject *type = Py_TYPE(src);

	if (type == &PyString_Type) {
		dst->type = FGW_STR | FGW_DYN;
		dst->val.str = fgw_strdup(PyString_AsString(PyObject_Str(src)));
	}
	else if (type == &PyFloat_Type) {
		dst->type = FGW_DOUBLE;
		dst->val.nat_double = PyFloat_AsDouble(src);
	}
	else if (type == &PyInt_Type) {
		dst->type = FGW_INT;
		dst->val.nat_int = PyInt_AS_LONG(src);
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

static const char *fgws_python_print_ptr(void *ptr)
{
	static char buf[64];
	sprintf(buf, "%p", ptr);
	return buf;
}

PyObject *fgws_python_arg2obj(fgw_ctx_t *fctx, fgw_arg_t *arg)
{
#	define FGW_PY_SET_LONG(lst, val)       return PyLong_FromLong(val);
#	define FGW_PY_SET_DOUBLE(lst, val)     return PyFloat_FromDouble(val);
#	define FGW_PY_SET_PTR(lst, val)        return PyString_FromString(fgws_python_print_ptr(val));
#	define FGW_PY_SET_STR(lst, val)        return PyString_FromString(val);
#	define FGW_PY_SET_NIL(lst, val)        return Py_False;

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_PY_SET_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_PY_SET_DOUBLE);
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
	py_func_t *func_ctx = (py_func_t *)self;
	fgw_func_t *func;
	int argc, n;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_error_t err;
	PyObject *py_res;

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
	PyObject *new;
	PyMethodDef *new_func_define;
	char *new_name = fgw_strdup(name);
	PyMethodDef met[] = {
		{NULL, fgws_python_call_fgw, METH_VARARGS, NULL},
		{NULL, NULL, 0, NULL}
	};

	met->ml_name = new_name;
	new_func_define = malloc(sizeof(met));
	memcpy(new_func_define, met, sizeof(met));

	new = PyCFunction_New(new_func_define, NULL);

	/* Misuse m_self to assing a 'self' to the function. Easy access to function name later. */
	func_ctx = malloc(sizeof(py_func_t));
	func_ctx->name = new_name;
	func_ctx->obj = obj;
	func_ctx->ctx = ctx;
	((PyCFunctionObject *)new)->m_self = (PyObject *)func_ctx;

	if ((new == NULL) || PyDict_SetItemString(ctx->root_dict, new_name, new)) {
		fgw_async_error(obj, "fgws_python_reg_func: failed to register function:");
		fgw_async_error(obj, new_name);
		fgw_async_error(obj, "\n");

		if (PyErr_Occurred())
			PyErr_Print();

		free(new_name);
		free(func_ctx);
		return;
	}

	Py_DECREF(new);
}

/* API: fgw calls a python function */
static fgw_error_t fgws_python_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	py_ctx_t *ctx = obj->script_data;
	PyObject *pfunc, *pargs, *pv, *pret;
	const char *func_name = argv[0].val.func->name;
	int n;

	pfunc = PyDict_GetItemString(ctx->dict, func_name);

	if ((pfunc == NULL) || !PyCallable_Check(pfunc)) {
		fgw_async_error(obj, "Not a callable python object:");
		fgw_async_error(obj, func_name);
		fgw_async_error(obj, "\n");
		if (PyErr_Occurred())
			PyErr_Print();
		return FGW_ERR_NOT_FOUND;
	}

	/* Create the args-list */
	pargs = PyTuple_New(argc-1);

	for (n = 1; n < argc; n++ ) {
		pv = fgws_python_arg2obj(obj->parent, &argv[n]);
		PyTuple_SetItem(pargs, n-1, pv);
		Py_DECREF(pv);
	}

	fgws_ucc_save(obj);
	pret = PyObject_CallObject(pfunc, pargs);
	fgws_ucc_restore(obj);

	if (pret != NULL)
		fgws_python_obj2arg(res, pret);
	else
		res->type = FGW_INVALID;

	Py_DECREF(pargs);
	if (pret != NULL) {
		Py_DECREF(pret);
	}

	if (PyErr_Occurred())
		PyErr_Print();
#warning TODO: return value
}

static int py_global_inited = 0;

/* API: unload the script */
static int fgws_python_unload(fgw_obj_t *obj)
{
	py_ctx_t *ctx = obj->script_data;

	/* Free all registrations */
	{
		PyCFunctionObject *func;
		PyMethodDef *ml;
		PyDictObject *mp;
		PyDictEntry *ep, *table;
		int fill;
		py_func_t *func_ctx;

		mp = (PyDictObject *)(ctx->root_dict);
		table = mp->ma_table;
		fill = mp->ma_fill;

		/* Go through all dict-items */
		for (ep = table; fill > 0; ep++) {
			if (ep->me_key == NULL)
				continue;

			fill--;

			/* Is it a valid function */
			if (!PyCallable_Check(ep->me_value))
				continue;

			func = (PyCFunctionObject *)(ep->me_value);
			ml = func->m_ml;

			if ((ml == NULL) || (func->m_self == NULL) || (ml->ml_meth != fgws_python_call_fgw))
				continue; /* not registered here */

			func_ctx = (py_func_t *)func->m_self;
			func->m_self = NULL;

			PyDict_DelItemString(ctx->root_dict, func_ctx->name);
			free(func_ctx->name);
			free(func_ctx);
		}
	}

	/* free global objects */
	if (ctx->module != NULL) {
/* ... but python2.7 sometimes crashes if the root module is dereferenced */
#if 0
		Py_DECREF(ctx->module);
		Py_DECREF(ctx->root_module);
#endif
	}
	free(ctx);

	py_global_inited--;
	if (py_global_inited == 0)
		Py_Finalize();
	return 0;
}

/* Helper function for the script to register its functions */
static PyObject *fgws_python_freg(PyObject *self, PyObject *args)
{
	py_func_t *func_ctx = (py_func_t *)self;
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

	name = PyString_AsString(PyObject_Str(PyTuple_GetItem(args, 0)));
	if (name == NULL) {
		fgw_async_error(obj, "fgw_func_reg: empty name\n");
		return 0;
	}

	pfunc = PyDict_GetItemString(func_ctx->ctx->root_dict, name);
	if (pfunc == NULL) {
		fgw_async_error(obj, "fgw_func_reg: function doesn't exist:");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
		if (PyErr_Occurred())
			PyErr_Print();
		return 0;
	}

	func = fgw_func_reg(obj, name, fgws_python_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_python_func_reg: failed to register function: ");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
		return 0;
	}

	return Py_True;
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
	obj->script_data =ctx;

	if (py_global_inited == 0) {
		Py_Initialize();
		PyRun_SimpleString("import sys");
	}

	sprintf(ctx->modname, "_fungw_%ld_", py_global_cnt++);
	ctx->root_module = Py_InitModule(ctx->modname, python_empty_method);
	ctx->root_dict = PyModule_GetDict(ctx->root_module);
	py_global_inited++;

	/* add the python->fgw glue */
	{
		py_ctx_t *ctx = obj->script_data;
		py_func_t *func_ctx;
		PyObject *new;
		PyMethodDef *new_func_define;
		char *new_name = fgw_strdup("fgw_func_reg");
		PyMethodDef met[] = {
			{"fgw_func_reg", fgws_python_freg, METH_VARARGS, NULL},
			{NULL, NULL, 0, NULL}
		};

		met->ml_name = new_name;
		new_func_define = malloc(sizeof(met));
		memcpy(new_func_define, met, sizeof(met));

		new = PyCFunction_New(new_func_define, NULL);

		/* Misuse m_self to assing a 'self' to the function. Easy access to function name later. */
		func_ctx = malloc(sizeof(py_func_t));
		func_ctx->name = new_name;
		func_ctx->obj = obj;
		func_ctx->ctx = ctx;
		((PyCFunctionObject *)new)->m_self = (PyObject *)func_ctx;

		if ((new == NULL) || PyDict_SetItemString(ctx->root_dict, new_name, new)) {
			fgw_async_error(obj, "fgws_python_init: failed to register function: fgw_func_reg\n");

			if (PyErr_Occurred())
				PyErr_Print();

			free(new_name);
			free(func_ctx);
			return -1;
		}

		Py_DECREF(new);
	}

	return 0;
}

/* API: load a script into an object */
static int fgws_python_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	py_ctx_t *ctx = obj->script_data;
	PyObject *modules = PyImport_GetModuleDict();
	PyObject *imp = PyDict_GetItemString(modules, "imp");

	if (imp == NULL) {
		imp = PyImport_ImportModule("imp");
		if (imp == NULL)
			abort();
	}
	else {
		Py_INCREF(imp);
	}

	ctx->module = PyObject_CallMethod(imp, "load_source", "ss", ctx->modname, filename);

	Py_INCREF(imp);

	if (ctx->module == NULL) {
		fgw_async_error(obj, "Failed to load python script:");
		fgw_async_error(obj, filename);
		fgw_async_error(obj, "\n");

		if (PyErr_Occurred())
			PyErr_Print();

		return -1;
	}

	ctx->dict = PyModule_GetDict(ctx->module);

	return 0;
}

static int fgws_python_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".py", ".python", NULL };
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
