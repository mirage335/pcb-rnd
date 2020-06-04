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
#include <estutter/stutter.h>

typedef struct fctx_s fctx_t;
struct fctx_s {
	fctx_t *next; /* linked list of all names so they can be freed */
	char name[1]; /* dynamic allocation */
};

typedef struct {
	fgw_obj_t *obj;
	stt_ctx_t stt;
	stt_varctx_t *vctx;
	fctx_t *fl; /* list of functions */
} estutter_t;

static void fgws_stt_obj2arg(fgw_arg_t *dst, const stt_obj_t *src)
{
	switch (src->type) {
		case STT_ST_NIL:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
			break;

		case STT_ST_INT:
			dst->type = FGW_LONG;
			dst->val.nat_long = src->d.integer.value;
			break;

		case STT_ST_NUM:
			dst->type = FGW_DOUBLE;
			dst->val.nat_double = src->d.num.value;
			break;

		case STT_ST_CHAR:
			dst->type = FGW_INT;
			dst->val.nat_int = src->d.ch.ch;
			break;

		case STT_ST_T:
			dst->type = FGW_INT;
			dst->val.nat_int = 1;
			break;

		case STT_ST_STRING:
			dst->type = FGW_STR;
			dst->val.str = src->d.string.value;
			break;

		default:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
	}
}

static stt_obj_t *fgws_stt_arg2obj(fgw_ctx_t *fctx, stt_ctx_t *stt, fgw_arg_t *arg)
{
#	define FGW_STT_RET_LONG(lst, val)   return stt_create_int(stt, val);
#	define FGW_STT_RET_DOUBLE(lst, val) return stt_create_num(stt, val);
#	define FGW_STT_RET_PTR(lst, val)    return stt_create_int(stt, (stt_intptr_t)val);
#	define FGW_STT_RET_STR(lst, val)    return stt_create_string(stt, val);
#	define FGW_STT_RET_NIL(lst, val)    return stt_create_nil(stt);

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_STT_RET_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_STT_RET_DOUBLE);
		ARG_CONV_CASE_DOUBLE(NULL, FGW_STT_RET_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(NULL, FGW_STT_RET_DOUBLE);
		ARG_CONV_CASE_PTR(NULL, FGW_STT_RET_PTR);
		ARG_CONV_CASE_STR(NULL, FGW_STT_RET_STR);
		ARG_CONV_CASE_CLASS(NULL, FGW_STT_RET_NIL);
		ARG_CONV_CASE_INVALID(NULL, FGW_STT_RET_NIL);
	}
	if (arg->type & FGW_PTR) {
		FGW_STT_RET_PTR(NULL, arg->val.ptr_void);
	}
	else
		FGW_STT_RET_NIL(NULL, 0);
}

/* API: the script is calling an fgw function */
static STT_BUILTIN(fgws_stt_call_fgw)
{
	estutter_t *est = ctx->stt->user_data;
	fctx_t *fctx = data;                  /* fctx->name */
	fgw_func_t *func;
	int argc, n;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_error_t err;
	stt_obj_t *sres, *sobj;

	func = fgw_func_lookup(est->obj->parent, fctx->name);
	if (func == NULL) {
		fgw_async_error(est->obj, "fgws_stt_call_fgw: function to be called is not found:");
		fgw_async_error(est->obj, fctx->name);
		fgw_async_error(est->obj, "\n");
		return STT_NIL(ctx->stt);
	}

	for(argc = 0, sobj = parm; sobj->type == STT_ST_CONS; sobj = sobj->d.cons.cdr)
		argc++;
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = est->obj->script_user_call_ctx;

	/* Convert all params to fgw args */
	for (n = 0; n < argc; n++) {
		stt_nextarg(ctx, &sobj, &parm);
		fgws_stt_obj2arg(&argv[n+1], sobj);
	}

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc+1, argv);

	/* Free the array */
	fgw_argv_free(est->obj->parent, argc, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return 0;

	sres = fgws_stt_arg2obj(func->obj->parent, &est->stt, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return sres;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_stt_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	estutter_t *est = obj->script_data;
	size_t len = strlen(name);
	fctx_t *fctx;
	char *s;
	stt_var_t *old;

	fctx = malloc(sizeof(fctx_t) + len); /* +1 for the \0 is granted by name[1] in the struct */
	memcpy(fctx->name, name, len+1);

	for(s = fctx->name; *s != '\0'; s++)
		if (*s == '.')
			*s = '-';

	old = stt_varctx_retr(est->vctx, fctx->name);
	if (old != NULL) {
		fprintf(stderr, "fungw_estutter fgws_stt_reg_func(): Can't register function over existing symbol: %s\n", fctx->name);
		free(fctx);
		return;
	}

	stt_varctx_set(est->vctx, fctx->name, stt_create_builtin_with_data(&est->stt, fgws_stt_call_fgw, fctx));

	fctx->next = est->fl;
	est->fl = fctx;
}


/* API: unload the script */
static int fgws_stt_unload(fgw_obj_t *obj)
{
	estutter_t *est = obj->script_data;
	fctx_t *f, *next;

	/* free all stored function names */
	for(f = est->fl; f != NULL; f = next) {
		next = f->next;
		free(f);
	}

	stt_varctx_free(est->vctx);
	stt_uninit(&est->stt);
	free(est);
	return 0;
}

/* API: fgw calls a stt function */
static fgw_error_t fgws_stt_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	estutter_t *est = obj->script_data;
	int n;
	stt_s_mark mark;
	stt_obj_t *call, *tail, *sres, *a;

	mark = stt_gc_prot_mark(&est->stt);

	call = stt_create_cons(&est->stt, stt_create_symbol(&est->stt, argv[0].val.func->name), NULL);
	tail = call;

	for(n = 1; n < argc; n++) {
		a = fgws_stt_arg2obj(obj->parent, &est->stt, &argv[n]);
		tail->d.cons.cdr = stt_create_cons(&est->stt, a, NULL);
		tail = tail->d.cons.cdr;
	}

	tail->d.cons.cdr = stt_create_nil(&est->stt);

	fgws_ucc_save(obj);
	sres = stt_eval(est->vctx, call);
	fgws_ucc_restore(obj);

	if (sres->type == STT_ST_ERROR) {
#warning TODO
		stt_fp_printobj(stderr, est->vctx, sres);
		return FGW_ERR_UNKNOWN;
	}

	stt_gc_prot_free(&est->stt, mark, NULL);
	fgws_stt_obj2arg(res, sres);
	return FGW_SUCCESS;
}

/* Helper function for the script to register its functions */
static STT_BUILTIN(fgws_stt_freg)
{
	estutter_t *est = ctx->stt->user_data;
	fgw_func_t *func;
	stt_obj_t *fna;

	STT_ARG_STRING(&est->stt, fna);

	func = fgw_func_reg(est->obj, fna->d.string.value, fgws_stt_call_script);
	if (func == NULL) {
		fgw_async_error(est->obj, "fgw_stt_func_reg: failed to register function\n");
		fgw_async_error(est->obj, fna->d.string.value);
		fgw_async_error(est->obj, "\n");
		return stt_create_nil(&est->stt);
	}

	return stt_create_t(&est->stt);
}

/* API: init the interpreter so that functions can be registered */
static int fgws_stt_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	estutter_t *est;

	est = calloc(sizeof(estutter_t), 1);
	est->stt.user_data = est;
	est->obj = obj;
	stt_init(&est->stt);
	est->vctx = stt_varctx_create(&est->stt, NULL, 1024);
	obj->script_data = est;
	stt_register_builtins(est->vctx);
	stt_register_stdio(est->vctx);

	/* add the stt->fgw glue */
	stt_varctx_set(est->vctx, "fgw_func_reg", stt_create_builtin(&est->stt, fgws_stt_freg));

	return 0;
}

/* API: load a script into an object */
static int fgws_stt_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	estutter_t *est = obj->script_data;
	stt_obj_t *sys;

	sys = stt_load_system(est->vctx);
	if(sys->type == STT_ST_ERROR) {
		stt_fp_printobj(stderr, est->vctx, sys);
		fputc('\n', stderr);
	}

	stt_evalfile(est->vctx, filename);

	return 0;
}

static int fgws_stt_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".estt", ".stt", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_estutter_eng = {
	"estutter",
	fgws_stt_call_script,
	fgws_stt_init,
	fgws_stt_load,
	fgws_stt_unload,
	fgws_stt_reg_func,
	NULL,
	fgws_stt_test_parse
};

int pplg_check_ver_fungw_estutter(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_estutter(void)
{
	fgw_eng_reg(&fgw_estutter_eng);
	return 0;
}

void pplg_uninit_fungw_estutter(void)
{
	fgw_eng_unreg(fgw_estutter_eng.name);
}
