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
#include <duktape.h>

#define FGW_CTX2OBJ_NAME "__fungw_ctx_to_obj__"

static fgw_obj_t *fgws_duk_ctx2obj(duk_context *ctx)
{
	fgw_obj_t *res;
	duk_get_global_string(ctx, FGW_CTX2OBJ_NAME);
	res = duk_to_pointer(ctx, -1);
	duk_pop(ctx);
	return res;
}

static duk_ret_t fgws_duk_print(duk_context *ctx)
{
	duk_push_string(ctx, " ");
	duk_insert(ctx, 0);
	duk_join(ctx, duk_get_top(ctx) - 1);
	printf("%s\n", duk_safe_to_string(ctx, -1));
	return 0;
}


static void fgws_duk_push_arg(fgw_ctx_t *fctx, duk_context *ctx, fgw_arg_t *arg)
{
#	define FGW_DUK_PUSH_LONG(lst, val)       duk_push_int(ctx, val); return;
#	define FGW_DUK_PUSH_DOUBLE(lst, val)     duk_push_number(ctx, val); return;
#	define FGW_DUK_PUSH_PTR(lst, val)        duk_push_pointer(ctx, val); return;
#	define FGW_DUK_PUSH_STR(lst, val)        duk_push_string(ctx, val); return;
#	define FGW_DUK_PUSH_NIL(lst, val)        duk_push_null(ctx); return;

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_DUK_PUSH_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_DUK_PUSH_DOUBLE);
		ARG_CONV_CASE_DOUBLE(NULL, FGW_DUK_PUSH_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(NULL, FGW_DUK_PUSH_DOUBLE);
		ARG_CONV_CASE_PTR(NULL, FGW_DUK_PUSH_PTR);
		ARG_CONV_CASE_STR(NULL, FGW_DUK_PUSH_STR);
		ARG_CONV_CASE_CLASS(NULL, FGW_DUK_PUSH_NIL);
		ARG_CONV_CASE_INVALID(NULL, FGW_DUK_PUSH_NIL);
	}

	duk_push_null(ctx);
}

static void fgws_duk_js2arg(duk_context *ctx, fgw_arg_t *dst, int src_idx)
{
	int type = duk_get_type(ctx, src_idx);
	switch(type) {
		case DUK_TYPE_BOOLEAN:
			dst->type = FGW_INT;
			dst->val.nat_int = duk_to_boolean(ctx, src_idx);
			break;
		case DUK_TYPE_NUMBER:
			dst->type = FGW_DOUBLE;
			dst->val.nat_double = duk_to_number(ctx, src_idx);
			break;
		case DUK_TYPE_STRING:
			dst->type = FGW_STR | FGW_DYN;
			dst->val.str = fgw_strdup(duk_to_string(ctx, src_idx));
			break;
		case DUK_TYPE_BUFFER:
			dst->type = FGW_STR | FGW_DYN;
			dst->val.str = fgw_strdup(duk_buffer_to_string(ctx, src_idx));
			break;
		case DUK_TYPE_POINTER:
			dst->type = FGW_PTR;
			dst->val.ptr_void = duk_to_pointer(ctx, src_idx);
			break;

		case DUK_TYPE_UNDEFINED:
		case DUK_TYPE_LIGHTFUNC:
		case DUK_TYPE_OBJECT:
			/* can't convert these */
			fprintf(stderr, "fgws_duk_js2arg: ignoring unconvertable type %d\n", type);

		case DUK_TYPE_NONE:
		case DUK_TYPE_NULL:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
			break;
	}
}

/* API: the script is calling an fgw function */
static duk_ret_t fgws_duk_call_fgw(duk_context *ctx)
{
	fgw_func_t *f;
	int n, argc;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_error_t err;
	fgw_obj_t *obj = fgws_duk_ctx2obj(ctx);

	/* figure the fgw function to call */
	duk_push_current_function(ctx);
	duk_get_prop_string(ctx, -1, "fgw_func");
	f = duk_to_pointer(ctx, -1);
	duk_pop_2(ctx);

	argc = duk_get_top(ctx);
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = f;
	argv[0].val.argv0.user_call_ctx = obj->script_user_call_ctx;

	for(n = 1; n <= argc; n++)
		fgws_duk_js2arg(ctx, &argv[n], -n);

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = f->func(&res, argc+1, argv);

	for(n = 1; n <= argc; n++)
		fgw_arg_free(obj->parent, &argv[n]);

	/* Free the array */
	fgw_argv_free(f->obj->parent, argc, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0) {
		/* return DUK_RET_ERROR; but this aborts */
		return 0;
	}

	if ((res.type == FGW_PTR) && (res.val.ptr_void == NULL))
		return 0;

	fgws_duk_push_arg(f->obj->parent, ctx, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return 1;
}

static int fgws_duk_freg_in_script(duk_context *ctx, const char *name, duk_ret_t (*f)(duk_context *ctx), void *fgw_func)
{
	duk_push_c_function(ctx, f, DUK_VARARGS);
	duk_push_pointer(ctx, fgw_func);
	duk_put_prop_string(ctx, -2, "fgw_func");
	duk_put_global_string(ctx, name);
	return 0;
}

/* API: fgw calls a duk function */
static fgw_error_t fgws_duk_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	duk_context *ctx = obj->script_data;
	int n;

	duk_get_global_string(ctx, argv[0].val.func->name);
	for(n = 1; n < argc; n++)
		fgws_duk_push_arg(obj->parent, ctx, &argv[n]);

	fgws_ucc_save(obj);
	duk_call(ctx, argc-1);
	fgws_ucc_restore(obj);

	fgws_duk_js2arg(ctx, res, -1);

	duk_pop(ctx); /* result */
	return 0;
}

/* Helper function for the script to register its functions */
static int fgws_duk_freg_in_fungw(duk_context *ctx)
{
	const char *fn;
	int argc = duk_get_top(ctx);
	fgw_obj_t *obj = fgws_duk_ctx2obj(ctx);
	fgw_func_t *func;

	if (argc != 1) {
		fprintf(stderr, "fgw_func_reg: called with wrong number of arguments (must be 1)\n");
		goto error;
	}

	fn = duk_get_string(ctx, -1);
	if (duk_get_global_string(ctx, fn) == 0) {
		fgw_async_error(obj, "fgw_func_reg: function does not exist:");
		fgw_async_error(obj, fn);
		fgw_async_error(obj, "\n");
		goto error;
	}
	
	if (!duk_is_function(ctx, -1)) {
		fgw_async_error(obj, "fgw_func_reg: not a function: ");
		fgw_async_error(obj, fn);
		fgw_async_error(obj, "\n");
		goto error;
	}

	func = fgw_func_reg(obj, fn, fgws_duk_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_func_reg: failed to register function\n");
		fgw_async_error(obj, fn);
		fgw_async_error(obj, "\n");
		return 0;
	}

	duk_push_true(ctx);
	return 1;

	error:;
	return 0;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_duk_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	fgws_duk_freg_in_script(obj->script_data, name, fgws_duk_call_fgw, f);
}


/* API: unload the script */
static int fgws_duk_unload(fgw_obj_t *obj)
{
	duk_context *ctx = obj->script_data;
	duk_destroy_heap(ctx);
	return 0;
}



/* API: init the interpreter so that functions can be registered */
static int fgws_duk_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	duk_context *ctx = duk_create_heap_default();
	if (ctx == NULL)
		return -1;
	obj->script_data = ctx;

	/* add the duk->fgw glue */
	fgws_duk_freg_in_script(ctx, "print", fgws_duk_print, NULL);
	fgws_duk_freg_in_script(ctx, "fgw_func_reg", fgws_duk_freg_in_fungw, NULL);

	duk_push_pointer(ctx, obj);
	duk_put_global_string(ctx, FGW_CTX2OBJ_NAME);
	return 0;
}

/* API: load a script into an object */
static int fgws_duk_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	duk_context *ctx = obj->script_data;
	FILE *f;
	char buf[4096];
	size_t ret;

	duk_push_string(ctx, filename);
	f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "failed to open '%s' for read\n", filename);
		return -1;
	}

	for(;;) {
		if (ferror(f)) {
			fclose(f);
			fprintf(stderr, "ferror when reading file '%s'\n", filename);
			return -1;
		}
		if (feof(f))
			break;

		ret = fread(buf, 1, sizeof(buf), f);
		if (ret == 0) {
			break;
		}

		duk_push_lstring(ctx, (const char *)buf, ret);
	}

	duk_concat(ctx, duk_get_top(ctx) - 1); /* -1 for filename */
	duk_insert(ctx, -2);
	duk_compile(ctx, 0);
	duk_call(ctx, 0);
	duk_pop(ctx); /* result */
	return 0;
}

static int fgws_duk_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".js", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_duk_eng = {
	"duktape",
	fgws_duk_call_script,
	fgws_duk_init,
	fgws_duk_load,
	fgws_duk_unload,
	fgws_duk_reg_func,
	NULL,
	fgws_duk_test_parse
};

int pplg_check_ver_fungw_duktape(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_duktape(void)
{
	fgw_eng_reg(&fgw_duk_eng);
	return 0;
}

void pplg_uninit_fungw_duktape(void)
{
	fgw_eng_unreg(fgw_duk_eng.name);
}
