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
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>

typedef struct {
	mrb_state *mrb;
	mrb_value script;
} mruby_ctx_t;

static void fgws_mruby_val2arg(mruby_ctx_t *ctx, fgw_arg_t *dst, mrb_value src)
{
	const char *s;

	switch(mrb_type(src)) {
		case MRB_TT_FALSE:
			dst->type = FGW_INT;
			dst->val.nat_int = 0;
			break;
		case MRB_TT_TRUE:
			dst->type = FGW_INT;
			dst->val.nat_int = 1;
			break;
		case MRB_TT_FIXNUM:
			dst->type = FGW_LONG;
			dst->val.nat_long = mrb_fixnum(src);
			break;
		case MRB_TT_FLOAT:
			dst->type = FGW_DOUBLE;
			dst->val.nat_double = mrb_float(src);
			break;
		case MRB_TT_CPTR:
			dst->type = FGW_PTR;
			dst->val.ptr_void = mrb_cptr(src);
			break;
		case MRB_TT_STRING:
			dst->type = FGW_STR | FGW_DYN;
			s = mrb_string_value_ptr(ctx->mrb, src);
			if (*s == '"') { /* mruby returns quoted string for some reason */
				dst->val.str = fgw_strdup(s+1);
				dst->val.str[strlen(dst->val.str)-1] = '\0';
			}
			else
				dst->val.str = fgw_strdup(s);
			break;
		default:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
			break;
	}
}

static void fgws_mruby_arg2val(fgw_ctx_t *fctx, mruby_ctx_t *ctx, mrb_value *dst, fgw_arg_t *arg)
{
#	define FGW_RB_SET_LONG(lst, val)       *dst = mrb_fixnum_value(val); return;
#	define FGW_RB_SET_DOUBLE(lst, val)     *dst = mrb_float_value(ctx->mrb, val); return;
#	define FGW_RB_SET_PTR(lst, val)        *dst = mrb_cptr_value(ctx->mrb, val); return;
#	define FGW_RB_SET_STR(lst, val)        *dst = mrb_str_new_cstr(ctx->mrb, val); return;
#	define FGW_RB_SET_NIL(lst, val)        *dst = mrb_nil_value(); return;

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

	*dst = mrb_nil_value();
}

/* API: the script is calling an fgw function */
static mrb_value fgws_mruby_call_fgw(mrb_state *mrb, mrb_value self)
{
	fgw_obj_t *obj = mrb->ud;
	mruby_ctx_t *ctx = obj->script_data;
	const char *func_name = mrb_sym2name(mrb, mrb_obj_to_sym(mrb, mrb_funcall(mrb, self, "__method__", 0)));
	fgw_arg_t res, *argv, argv_static[16];
	fgw_func_t *func;
	int n, argc;
	mrb_value *rargv, rres;
	fgw_error_t err;

	/* Find our proc based on the function-name */
	func = fgw_func_lookup(obj->parent, func_name);

	if (func == NULL) {
		fgw_async_error(obj, "fgws_mruby_call_fgw: function to be called is not found:");
		fgw_async_error(obj, func_name);
		fgw_async_error(obj, "\n");
		return mrb_nil_value();
	}

	mrb_get_args(mrb, "*", &rargv, &argc);
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = obj->script_user_call_ctx;

	/* Convert all params to a string */
	for (n = 0; n < argc; n++)
		fgws_mruby_val2arg(ctx, &argv[n+1], mrb_inspect(mrb, rargv[n]));

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc+1, argv);

	/* Free the array */
	fgw_argv_free(obj->parent, argc+1, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return mrb_nil_value();

	fgws_mruby_arg2val(func->obj->parent, ctx, &rres, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return rres;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_mruby_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	mruby_ctx_t *ctx = obj->script_data;
	mrb_define_method(ctx->mrb, ctx->mrb->object_class, name, fgws_mruby_call_fgw, MRB_ARGS_ANY());
}

/* API: fgw calls a mruby function */
static fgw_error_t fgws_mruby_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	mruby_ctx_t *ctx = obj->script_data;
	const char *func_name = argv[0].val.func->name;
	mrb_value *rargv, rres;
	int n;
	mrb_sym rfun = mrb_intern_cstr(ctx->mrb, func_name);

	rargv = malloc(sizeof(mrb_value) * argc);

	for(n = 1; n < argc; n++)
		fgws_mruby_arg2val(obj->parent, ctx, &rargv[n-1], &argv[n]);

	fgws_ucc_save(obj);
	rres = mrb_funcall_argv(ctx->mrb, ctx->script, rfun, argc-1, rargv);
	fgws_ucc_restore(obj);

	free(rargv);
	fgws_mruby_val2arg(ctx, res, rres);
	return 0;
}

/* API: unload the script */
static int fgws_mruby_unload(fgw_obj_t *obj)
{
	mruby_ctx_t *ctx = obj->script_data;
	mrb_close(ctx->mrb);
	free(ctx);
	return 0;
}

/* Helper function for the script to register its functions */
static mrb_value fgws_mruby_func_reg(mrb_state *mrb, mrb_value self)
{
	fgw_obj_t *obj = mrb->ud;
	mruby_ctx_t *ctx = obj->script_data;
	mrb_value *rargv;
	int argc;
	const char *s;
	char *func_name;
	fgw_func_t *func;

	mrb_get_args(mrb, "*", &rargv, &argc);

	if (argc != 1) {
		fgw_async_error(obj, "fgws_mruby_func_reg: wrong number of arguments: need 1\n");
		return mrb_false_value();
	}
	if (mrb_type(rargv[0]) != MRB_TT_STRING) {
		fgw_async_error(obj, "fgws_mruby_func_reg: wrong type of arguments: must be string\n");
		return mrb_false_value();
	}

	s = mrb_string_value_ptr(ctx->mrb, rargv[0]);
	if (*s == '"') { /* mruby returns quoted string for some reason */
		func_name = fgw_strdup(s+1);
		func_name[strlen(func_name)-1] = '\0';
	}
	else
		func_name = (char *)s;

	func = fgw_func_reg(obj, func_name, fgws_mruby_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_mruby_func_reg: failed to register function: ");
		fgw_async_error(obj, func_name);
		fgw_async_error(obj, "\n");
		if (func_name != s)
			free(func_name);
		return mrb_false_value();
	}

	if (func_name != s)
		free(func_name);

	return mrb_true_value();
}

/* API: init the interpreter so that functions can be registered */
static int fgws_mruby_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	mruby_ctx_t *ctx = calloc(sizeof(mruby_ctx_t), 1);

	ctx->mrb = mrb_open();
	ctx->mrb->ud = obj;
	obj->script_data = ctx;

	mrb_define_method(ctx->mrb, ctx->mrb->object_class, "fgw_func_reg", fgws_mruby_func_reg, MRB_ARGS_ANY());

	return 0;
}

/* API: load a script into an object */
static int fgws_mruby_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	mruby_ctx_t *ctx = obj->script_data;
	FILE *fp;

	/* Load the file, and execute once */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		fgw_async_error(obj, "fgws_mruby_load: filed to open file for reading: ");
		fgw_async_error(obj, filename);
		fgw_async_error(obj, "\n");
		return -1;
	}
	ctx->script = mrb_load_file(ctx->mrb, fp);
	fclose(fp);


#warning TODO:
/*	if (ruby_errinfo != Qnil)
		error(1, "[MRuby] Error (%s:%d): %s", ruby_sourcefile, ruby_sourceline,  RSTRING(rb_obj_as_string(ruby_errinfo))->ptr);*/

	return 0;

}

static int fgws_mruby_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".rb", ".mruby", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_mruby_eng = {
	"mruby",
	fgws_mruby_call_script,
	fgws_mruby_init,
	fgws_mruby_load,
	fgws_mruby_unload,
	fgws_mruby_reg_func,
	NULL,
	fgws_mruby_test_parse
};

int pplg_check_ver_fungw_mruby(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_mruby(void)
{
	fgw_eng_reg(&fgw_mruby_eng);
	return 0;
}

void pplg_uninit_fungw_mruby(void)
{
	fgw_eng_unreg(fgw_mruby_eng.name);
}
