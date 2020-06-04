/*
    fungw - language-agnostic function gateway
    Copyright (C) 2017, 2019  Tibor 'Igor2' Palinkas

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
#include <libfungw/fungw.h>
#include <libfungw/fungw_conv.h>
#include <libmawk.h>


#define MAX_ARG 256

typedef struct mawk_ctx_s {
	mawk_state_t *state;
	mawk_vio_t *vf_stdin;
} mawk_ctx_t;


/* Extract the fgw object from a mawk state (stored there as a global variable) */
static fgw_obj_t *fgws_mawk_get_obj(mawk_state_t *mst)
{
	return mst->func_userdata;
}


/* Convert fgw args to void * arg for a mawk call */
static void fgw_mawk_set_cell(fgw_ctx_t *fctx, mawk_ctx_t *mctx, mawk_cell_t *dst, fgw_arg_t *arg)
{
	char tmp[128];
#	define FGW_MAWK_SET_DOUBLE(lst, val)  dst->type = C_NUM, dst->d.dval = val; return;
#	define FGW_MAWK_SET_STR(lst, val)     dst->type = C_STRING, dst->ptr = mawk_new_STRING(mctx->state, val); return;
#	define FGW_MAWK_SET_PTR(lst, val)     sprintf(tmp, "%p", val); dst->type = C_STRING, dst->ptr = mawk_new_STRING(mctx->state, tmp); return;
#	define FGW_MAWK_SET_NIL(lst, val)     dst->type = C_NOINIT; return;

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(lst, FGW_MAWK_SET_DOUBLE);
		ARG_CONV_CASE_LLONG(lst, FGW_MAWK_SET_DOUBLE);
		ARG_CONV_CASE_DOUBLE(lst, FGW_MAWK_SET_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(lst, FGW_MAWK_SET_DOUBLE);
		ARG_CONV_CASE_PTR(lst, FGW_MAWK_SET_PTR);
		ARG_CONV_CASE_STR(lst, FGW_MAWK_SET_STR);
		ARG_CONV_CASE_CLASS(lst, FGW_MAWK_SET_NIL);
		ARG_CONV_CASE_INVALID(lst, FGW_MAWK_SET_NIL);
	}

	if (arg->type & FGW_PTR) {
		FGW_MAWK_SET_PTR(lst, arg->val.ptr_void);
	}
	else {
		FGW_MAWK_SET_NIL(dst, 0)
	}
}

/* Read the mawk stack and convert the result to an fgw arg */
static void fgw_mawk_get_cell(mawk_ctx_t *mctx, fgw_arg_t *dst, mawk_cell_t *src)
{
	switch(src->type) {
		case C_NUM:
		case C_STRNUM:
			dst->type = FGW_DOUBLE;
			dst->val.nat_double = src->d.dval;
			break;
		case C_STRING:
		case C_MBSTRN:
			dst->type = FGW_STR | FGW_DYN;
			dst->val.str = fgw_strdup(((mawk_string_t *)(src->ptr))->str);
			break;
		case C_NOINIT:
		default:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
			break;
	}
}

typedef struct {
	fgw_obj_t *obj;
	char fname[1];
} fgws_mawk_call_t;

/* API: the script is calling an fgw function */
static mawk_cell_t *fgws_mawk_call_fgw(mawk_state_t *context, mawk_cell_t *sp, int argc)
{
	int n, i;
	fgws_mawk_call_t *cl = context->func_userdata;
	fgw_arg_t res, *argv, argv_static[16];
	mawk_ctx_t *mctx = cl->obj->script_data;
	fgw_func_t *func = fgw_func_lookup(cl->obj->parent, cl->fname);

	if (cl == NULL) { /* happens when a function is deregistered */
		goto error;
	}

	mctx = cl->obj->script_data;
	func = fgw_func_lookup(cl->obj->parent, cl->fname);

	if (func == NULL) {
		error:;
		res.type = FGW_PTR;
		res.val.ptr_void = NULL;
		goto out;
	}

	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = cl->obj->script_user_call_ctx;

	for(n = 1, i = argc-1; i >= 0; i--,n++)
		fgw_mawk_get_cell(mctx, &argv[n], sp-i);

	/* Run command */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	if (func->func(&res, argc+1, argv) != FGW_SUCCESS) {
		res.type = FGW_PTR;
		res.val.ptr_void = NULL;
	}

	/* Free the array */
	fgw_argv_free(cl->obj->parent, argc+1, argv);
	if (argv != argv_static)
		free(argv);

	/* restore stack */
	out:;
	fgw_mawk_set_cell(func->obj->parent, mctx, libmawk_cfunc_ret(sp, argc), &res);
	return sp-argc;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_mawk_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	mawk_ctx_t *mctx = obj->script_data;
	fgws_mawk_call_t *cl;
	void *old_userdata;
	int nl = strlen(name);

	old_userdata = mctx->state->func_userdata;

	cl = malloc(sizeof(fgws_mawk_call_t)+nl); /* the +1 for \0 is the sizeof(cl->fname) */
	cl->obj = obj;
	memcpy(cl->fname, name, nl+1);
	mctx->state->func_userdata = cl;

	libmawk_register_function(mctx->state, name, fgws_mawk_call_fgw);

	mctx->state->func_userdata = old_userdata;
}

static void fgws_mawk_unreg_func(fgw_obj_t *obj, const char *name)
{
	mawk_ctx_t *mctx = obj->script_data;
	SYMTAB *sym;

	if ((mctx == NULL) || (mctx->state == NULL))
		return;

	sym = mawk_find(mctx->state, name, 0);

	if ((sym != NULL) && (sym->type == ST_C_FUNCTION)) {
		fgws_mawk_call_t *cl = sym->stval.c_function.func_userdata;
		free(cl);
		sym->stval.c_function.func_userdata = NULL;
	}
}


/* API: fgw calls a mawk function */
static fgw_error_t fgws_mawk_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	mawk_ctx_t *mctx = obj->script_data;
	mawk_cell_t arg[MAX_ARG];
	mawk_cell_t retc = libmawk_empty_cell;
	mawk_exec_result_t er;
	int n;

	if (argc >= MAX_ARG)
		return FGW_ERR_ARGC;

	for(n = 1; n < argc; n++)
		fgw_mawk_set_cell(obj->parent, mctx, &arg[n-1], &argv[n]);

	fgws_ucc_save(obj);
	er = libmawk_call_functionc(mctx->state, argv[0].val.func->name, &retc, argc-1, arg);
	fgws_ucc_restore(obj);

	if (er == MAWK_EXER_FUNCRET) {
		fgw_mawk_get_cell(mctx, res, &retc);
		libmawk_cell_destroy(mctx->state, &retc);
		return FGW_SUCCESS;
	}

	res->type = FGW_INVALID;
	return FGW_ERR_UNKNOWN;
}



/* API: unload the script */
static int fgws_mawk_unload(fgw_obj_t *obj)
{
	mawk_ctx_t *mctx = obj->script_data;

	if ((mctx != NULL) && (mctx->state != NULL))
		libmawk_uninitialize(mctx->state);
	free(mctx);
	obj->script_data = NULL;
	return 0;
}

/* Helper function for the script to register its functions */
static mawk_cell_t *fgws_mawk_freg(mawk_state_t *mst, mawk_cell_t *sp, int argc)
{
	fgw_obj_t *obj;
	char name[FGW_ID_LEN];
	fgw_func_t *func;

	obj = fgws_mawk_get_obj(mst);

	if (argc != 1) {
		fgw_async_error(obj, "fgw_func_reg: wrong number of arguments: need 1\n");
		goto ret;
	}

	libmawk_print_cell(mst, libmawk_cfunc_arg(sp, argc, 0), name, sizeof(name));

	func = fgw_func_reg(obj, name, fgws_mawk_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_func_reg: failed to register function\n");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
	}

	ret:;
	return sp-argc;
}

/* API: init the interpreter so that functions can be registered */
static int fgws_mawk_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	mawk_ctx_t *mctx;

	/* initialize the mawk state */
	obj->script_data = mctx = calloc(sizeof(mawk_ctx_t), 1);
	mctx->state = libmawk_initialize_stage1();
	libmawk_initialize_stdio(mctx->state, 0, 1, 1);
	mctx->state->func_userdata = obj;

	if (mctx->state == NULL) {
		fgw_async_error(obj, "fgws_mawk_init: stage1 init failed\n");
		free(mctx);
		obj->script_data = NULL;
		return -1;
	}

	/* set up stdio */
	mawk_vio_orig_setup_stdio(mctx->state, 0, 1, 1);                     /* whether bind to the app's stdio: 0,1,1=stdin,stdout,stderr; let stdout and stderr bind */
	mctx->vf_stdin  = mawk_vio_fifo_open(mctx->state, NULL, MAWK_VIO_I); /* create a pipe for stdin */
	mawk_file_register(mctx->state, "/dev/stdin", F_IN, mctx->vf_stdin); /* register /dev/stdin */
	mctx->state->vio_init = mawk_vio_orig_init;                          /* file operation is handled by the orig vio */

	/* register freg before the script is loaded */
	libmawk_register_function(mctx->state, "fgw_func_reg", fgws_mawk_freg);
	return 0;
}

/* API: load a script into an object */
static int fgws_mawk_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	mawk_ctx_t *mctx = obj->script_data;

	/* load the script */
	mawk_append_input_file(mctx->state, filename, 0);
	mctx->state = libmawk_initialize_stage2(mctx->state, 0, NULL);
	if (mctx->state == NULL) {
		fgw_async_error(obj, "fgws_mawk_load: stage2 init failed: syntax error in the script?\n");
		goto error;
	}

	mctx->state = libmawk_initialize_stage3(mctx->state);
	if (mctx->state == NULL) {
		fgw_async_error(obj, "fgws_mawk_load: stage3 init failed: BEGIN of the script broke\n");
		goto error;
	}

	return 0;

	error:;
	free(mctx);
	obj->script_data = NULL;
	return -1;
}

static int fgws_mawk_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".awk", ".mawk", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_mawk_eng = {
	"mawk",
	fgws_mawk_call_script,
	fgws_mawk_init,
	fgws_mawk_load,
	fgws_mawk_unload,
	fgws_mawk_reg_func,
	fgws_mawk_unreg_func,
	fgws_mawk_test_parse
};

int pplg_check_ver_fungw_mawk(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_mawk(void)
{
	fgw_eng_reg(&fgw_mawk_eng);
	return 0;
}

void pplg_uninit_fungw_mawk(void)
{
	fgw_eng_unreg(fgw_mawk_eng.name);
}
