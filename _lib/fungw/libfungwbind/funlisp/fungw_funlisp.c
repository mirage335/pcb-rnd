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
#include <funlisp.h>

typedef struct fgws_funlist_fnc_s fgws_funlist_fnc_t;

struct fgws_funlist_fnc_s {
	fgw_obj_t *obj;
	fgws_funlist_fnc_t *next;
	char func_name[];
};

typedef struct {
	lisp_runtime *rt;
	lisp_scope *scp;
	fgws_funlist_fnc_t *func_ctxs;
} funlisp_ctx_t;

/* run the garbage collector to free all temporary objects */
static void fgws_funlisp_gc(funlisp_ctx_t *ctx)
{
	lisp_mark(ctx->rt, (lisp_value*)ctx->scp);
	lisp_sweep(ctx->rt);
}

static void fgws_funlisp_val2arg(funlisp_ctx_t *ctx, fgw_arg_t *dst, lisp_value *src)
{
	if (lisp_is(src, type_integer)) {
		dst->type = FGW_INT;
		dst->val.nat_int = lisp_integer_get((lisp_integer *)src);
	}
	else if (lisp_is(src, type_string)) {
		dst->type = FGW_STR;
		dst->val.str = lisp_string_get((lisp_string *)src);

	}
	else {
		dst->type = FGW_PTR;
		dst->val.ptr_void = NULL;
	}
}

static lisp_value *fgws_funlisp_arg2val(fgw_ctx_t *fctx, funlisp_ctx_t *ctx, fgw_arg_t *arg)
{
#	define FGW_RB_SET_LONG(lst, val)       return (lisp_value *)lisp_integer_new(ctx->rt, val);
#	define FGW_RB_SET_DOUBLE(lst, val)     return (lisp_value *)lisp_integer_new(ctx->rt, fungw_round(val));
#	define FGW_RB_SET_PTR(lst, val)        return NULL;
#	define FGW_RB_SET_STR(lst, val)        return (lisp_value *)lisp_string_new(ctx->rt, val, LS_CPY | LS_OWN);
#	define FGW_RB_SET_NIL(lst, val)        return lisp_nil_new(ctx->rt);

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_RB_SET_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_RB_SET_DOUBLE);
		ARG_CONV_CASE_DOUBLE(NULL, FGW_RB_SET_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(NULL, FGW_RB_SET_DOUBLE);
		ARG_CONV_CASE_PTR(NULL, FGW_RB_SET_PTR);
		ARG_CONV_CASE_STR(NULL, FGW_RB_SET_STR);
		ARG_CONV_CASE_CLASS(NULL, FGW_RB_SET_NIL);
		ARG_CONV_CASE_INVALID(NULL, FGW_RB_SET_NIL);
	}
	return lisp_nil_new(ctx->rt);
}


/* API: the script is calling an fgw function */
static lisp_value *fgws_funlisp_call_fgw(lisp_runtime *rt, lisp_scope *scope, lisp_list *arglist, void *user)
{
	fgws_funlist_fnc_t *fctx = user;
	fgw_obj_t *obj = fctx->obj;
	funlisp_ctx_t *ctx = obj->script_data;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_func_t *func;
	int n, argc;
	lisp_value *lres;
	fgw_error_t err;
	lisp_list *a;

	/* Find our proc based on the function-name */
	func = fgw_func_lookup(obj->parent, fctx->func_name);

	if (func == NULL) {
		fgw_async_error(obj, "fgws_funlisp_call_fgw: function to be called is not found:");
		fgw_async_error(obj, fctx->func_name);
		fgw_async_error(obj, "\n");
		return NULL;
	}

	a = arglist;
	if (a == NULL) {
		fgw_async_error(obj, "fgws_funlisp_call_fgw: failed to evaluate arguments for function call:");
		fgw_async_error(obj, fctx->func_name);
		fgw_async_error(obj, "\n");
		return NULL;
	}

	argc = lisp_list_length((lisp_list *)a);
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = obj->script_user_call_ctx;

	/* Convert all params to a string */
	for (n = 0; n < argc; n++) {
		fgws_funlisp_val2arg(ctx, &argv[n+1], lisp_list_get_left(a));
		a = (lisp_list *)lisp_list_get_right(a);
	}

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc+1, argv);

	/* Free the array */
	fgw_argv_free(obj->parent, argc, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return NULL;

	lres = fgws_funlisp_arg2val(func->obj->parent, ctx, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return lres;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_funlisp_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	fgws_funlist_fnc_t *fctx;
	funlisp_ctx_t *ctx = obj->script_data;
	int fnlen = strlen(name);

	fctx = malloc(sizeof(fgws_funlist_fnc_t) + fnlen + 1);
	fctx->obj = obj;
	memcpy(fctx->func_name, name, fnlen + 1);
	fctx->next = ctx->func_ctxs;
	ctx->func_ctxs = fctx;
	lisp_scope_add_builtin(ctx->rt, ctx->scp, fctx->func_name, fgws_funlisp_call_fgw, fctx, 1);
}

/* API: fgw calls a funlisp function */
static fgw_error_t fgws_funlisp_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	funlisp_ctx_t *ctx = obj->script_data;
	 char *func_name = argv[0].val.func->name;
	lisp_value *rres;
	int n;
	lisp_value *function = lisp_scope_lookup_string(ctx->rt, ctx->scp, func_name);
	lisp_list *largs, *tail;
	if (function == NULL)
		return FGW_ERR_NOT_FOUND;

	largs = tail = (lisp_list *)lisp_nil_new(ctx->rt);
	for(n = 1; n < argc; n++)
		lisp_list_append(ctx->rt, &largs, &tail, fgws_funlisp_arg2val(obj->parent, ctx, &argv[n]));

	fgws_ucc_save(obj);
	rres = lisp_call(ctx->rt, ctx->scp, function, largs);
	fgws_ucc_restore(obj);

	if (rres == NULL) {
		res->type = FGW_PTR;
		res->val.ptr_void = NULL;
	}
	else
		fgws_funlisp_val2arg(ctx, res, rres);

	fgws_funlisp_gc(ctx);
	return 0;
}

/* API: unload the script */
static int fgws_funlisp_unload(fgw_obj_t *obj)
{
	funlisp_ctx_t *ctx = obj->script_data;
	fgws_funlist_fnc_t *fctx, *fnext;

	lisp_runtime_free(ctx->rt);
	for(fctx = ctx->func_ctxs; fctx != NULL; fctx = fnext) {
		fnext = fctx->next;
		free(fctx);
	}
	free(ctx);
	return 0;
}

/* Helper function for the script to register its functions */
static lisp_value *fgws_funlisp_func_reg(lisp_runtime *rt, lisp_scope *scope, lisp_list *arglist, void *user)
{
	fgw_obj_t *obj = user;
	funlisp_ctx_t *ctx = obj->script_data;
	const char *func_name;
	fgw_func_t *func;
	lisp_value *fnv;

	if (!lisp_get_args(rt, arglist, "S", &fnv) || (!lisp_is(fnv, type_string))) {
		fgw_async_error(obj, "fgws_funlisp_func_reg: wrong arguments: need one string (function name)\n");
		return NULL;
	}

	func_name = lisp_string_get((lisp_string *)fnv);

	func = fgw_func_reg(obj, func_name, fgws_funlisp_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_funlisp_func_reg: failed to register function: ");
		fgw_async_error(obj, func_name);
		fgw_async_error(obj, "\n");
		return NULL;
	}

	return (lisp_value *)lisp_integer_new(ctx->rt, 1);
}

/* API: init the interpreter so that functions can be registered */
static int fgws_funlisp_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	funlisp_ctx_t *ctx = calloc(sizeof(funlisp_ctx_t), 1);

	ctx->rt = lisp_runtime_new();
	lisp_enable_symcache(ctx->rt);
	lisp_enable_strcache(ctx->rt);
	ctx->scp = lisp_new_default_scope(ctx->rt);

	obj->script_data = ctx;

	lisp_scope_add_builtin(ctx->rt, ctx->scp, "fgw_func_reg", fgws_funlisp_func_reg, obj, 1);
	return 0;
}

/* API: load a script into an object */
static int fgws_funlisp_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	funlisp_ctx_t *ctx = obj->script_data;
	FILE *fp;
	lisp_value *result;

	/* Load the file, and execute once */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		fgw_async_error(obj, "fgws_funlisp_load: filed to open file for reading: ");
		fgw_async_error(obj, filename);
		fgw_async_error(obj, "\n");
		return -1;
	}
	lisp_load_file(ctx->rt, ctx->scp, fp);
	fclose(fp);

	if (lisp_get_error(ctx->rt)) {
#warning TODO: error line
/*		char tmp[128];
		sprintf(tmp, "fgws_funlisp_load: filed in lisp_load_file() around line %d: ", ctx->rt->error_line)
		fgw_async_error(obj, tmp);*/
		fgw_async_error(obj, "fgws_funlisp_load: filed in lisp_load_file(): ");
		fgw_async_error(obj, lisp_get_error(ctx->rt));
		fgw_async_error(obj, "\n");
		return -1;
	}

	result = lisp_run_main_if_exists(ctx->rt, ctx->scp, 0, NULL);
	if (!result) {
#warning TODO: error line
		fgw_async_error(obj, "fgws_funlisp_load: filed to run main: ");
		fgw_async_error(obj, lisp_get_error(ctx->rt));
		fgw_async_error(obj, "\n");
		return -1;
	}

	fgws_funlisp_gc(ctx);
	return 0;
}

static int fgws_funlisp_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".fl", ".funlisp", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_funlisp_eng = {
	"funlisp",
	fgws_funlisp_call_script,
	fgws_funlisp_init,
	fgws_funlisp_load,
	fgws_funlisp_unload,
	fgws_funlisp_reg_func,
	NULL,
	fgws_funlisp_test_parse
};

int pplg_check_ver_fungw_funlisp(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_funlisp(void)
{
	fgw_eng_reg(&fgw_funlisp_eng);
	return 0;
}

void pplg_uninit_fungw_funlisp(void)
{
	fgw_eng_unreg(fgw_funlisp_eng.name);
}
