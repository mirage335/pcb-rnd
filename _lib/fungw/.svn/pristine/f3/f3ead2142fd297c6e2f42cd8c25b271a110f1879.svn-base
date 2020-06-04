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
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

/* Perl doesn't offer an official user data pointer in the interpereter
   struct, but in return the init function can't create variables or
   in the interpreter before parsing a script; this workaround abuses
   an unused field (as of perl 5.26.2). */
#define perl_user_data IXpv

typedef struct {
	PerlInterpreter *interp;
	int freg_delay;
	long fregs_used, fregs_alloced;
	char **fregs;
	fgw_obj_t *obj;
} perl_ctx_t;


static void fgws_perl_sv2arg(fgw_arg_t *dst, SV *src)
{
	if (SvIOK(src)) {
		dst->type = FGW_INT;
		dst->val.nat_int = SvIV(src);
	}
	else if (SvNOK(src)) {
		dst->type = FGW_DOUBLE;
		dst->val.nat_double = SvNV(src);
	}
	else if (SvPOK(src)) {
		dst->type = FGW_STR | FGW_DYN;
		dst->val.str = fgw_strdup(SvPV_nolen(src));
	}
	else {
		dst->type = FGW_PTR;
		dst->val.ptr_void = NULL;
		fprintf(stderr, "fgws_perl_sv2arg: ignoring unknown type\n");
	}
}

static SV *fgws_perl_arg2sv(fgw_ctx_t *fctx, perl_ctx_t *ctx, fgw_arg_t *arg)
{
#	define FGW_PUSH_LONG(lst, val)       return newSViv(val);
#	define FGW_PUSH_DOUBLE(lst, val)     return newSVnv(val);
#	define FGW_PUSH_PTR(lst, val)        return newSViv((long)val);
#	define FGW_PUSH_STR(lst, val)        return newSVpv(val, 0);
#	define FGW_PUSH_NIL(lst, val)        return newSVpv("", 0);

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO); /* if fails, it remains custom and will be unhandled */

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(NULL, FGW_PUSH_LONG);
		ARG_CONV_CASE_LLONG(NULL, FGW_PUSH_DOUBLE);
		ARG_CONV_CASE_DOUBLE(NULL, FGW_PUSH_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(NULL, FGW_PUSH_DOUBLE);
		ARG_CONV_CASE_PTR(NULL, FGW_PUSH_PTR);
		ARG_CONV_CASE_STR(NULL, FGW_PUSH_STR);
		ARG_CONV_CASE_CLASS(NULL, FGW_PUSH_NIL);
		ARG_CONV_CASE_INVALID(NULL, FGW_PUSH_NIL);
	}

	return newSVpv("", 0);
}


/* API: the script is calling an fgw function */
static XS(fgws_perl_call_fgw)
{
	perl_ctx_t *ctx = (perl_ctx_t *)my_perl->perl_user_data;
	fgw_func_t *func;
	GV *gv;
	SV *func_name_sv;
	const char *func_name;
	int argc, n;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_error_t err;
	dXSARGS;

	SP -= items;

	/* Figure the function name */
	gv = CvGV(cv);
	func_name_sv = sv_newmortal();
	gv_efullname3(func_name_sv, gv, Nullch);
	func_name = SvPV_nolen(func_name_sv) + 6; /* +6 is for truncating the "main::" prefix */

	/* Set up args */
	argc = items;
	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	/* Set the first param */
	func = fgw_func_lookup(ctx->obj->parent, func_name);
	if (func == NULL) {
		fgw_async_error(ctx->obj, "fgws_perl_call_fgw: function to be called is not found:");
		fgw_async_error(ctx->obj, func_name);
		fgw_async_error(ctx->obj, "\n");
		return XSRETURN_EMPTY;
	}
	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = ctx->obj->script_user_call_ctx;

	/* Convert all params to fgw args */
	for(n = 0; n < argc; n++)
		fgws_perl_sv2arg(&argv[n+1], ST(n));

	/* Call the target function */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc+1, argv);

	/* Free the array */
	fgw_argv_free(ctx->obj->parent, argc+1, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return XSRETURN_EMPTY;

	ST(0) = fgws_perl_arg2sv(func->obj->parent, ctx, &res);
	sv_2mortal(ST(0));

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return XSRETURN(1);
}

/* Helper function for the script to register its functions */
static void fgws_perl_freg(void *client_data, const char *name)
{
	perl_ctx_t *ctx = client_data;

	if (ctx->freg_delay) { /* still loading the script - delay command registrations */
		if (ctx->fregs_used >= ctx->fregs_alloced) {
			ctx->fregs_alloced += 32;
			ctx->fregs = realloc(ctx->fregs, sizeof(char *) * ctx->fregs_alloced);
		}
		ctx->fregs[ctx->fregs_used] = fgw_strdup(name);
		ctx->fregs_used++;
	}
	else {
		PERL_SET_CONTEXT(ctx->interp);
		newXS((char *)name, fgws_perl_call_fgw, __FILE__);
	}
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_perl_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	fgws_perl_freg(obj->script_data, name);
}

/* API: fgw calls a perl function */
static fgw_error_t fgws_perl_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	perl_ctx_t *ctx = obj->script_data;
	const char *func_name = argv[0].val.func->name;
	I32 ret;
	int n;
	dSP;

	PERL_SET_CONTEXT(ctx->interp);
	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	EXTEND(SP, argc-1);
	for(n = 1; n < argc; n++)
		PUSHs(sv_2mortal(fgws_perl_arg2sv(obj->parent, ctx, &argv[n])));
	PUTBACK;
	fgws_ucc_save(obj);
	ret = call_pv(func_name, G_SCALAR);
	fgws_ucc_restore(obj);
	SPAGAIN;
	if (ret != 1) {
		res->type = FGW_PTR;
		res->val.ptr_void = NULL;
		return FGW_ERR_UNKNOWN;
	}
	fgws_perl_sv2arg(res, POPs);
	PUTBACK;
	FREETMPS;
	LEAVE;
	return 0;
}

/* API: unload the script */
static int fgws_perl_unload(fgw_obj_t *obj)
{
	perl_ctx_t *ctx = obj->script_data;

	if (ctx->interp != NULL) {
		PERL_SET_CONTEXT(ctx->interp);
		perl_destruct(ctx->interp);
		perl_free(ctx->interp);
	}

	free(ctx);
	return 0;
}


/* fgw_func_reg() when called from the perl script - register a script function in fungw */
static XS(fgws_perl_func_reg)
{
	dXSARGS;
	const char *name;
	perl_ctx_t *ctx = (perl_ctx_t *)my_perl->perl_user_data;
	fgw_func_t *func;

	SP -= items;

	name = SvPV_nolen(ST(0));

	func = fgw_func_reg(ctx->obj, name, fgws_perl_call_script);
	if (func == NULL) {
		fgw_async_error(ctx->obj, "fgw_perl_func_reg: failed to register function: ");
		fgw_async_error(ctx->obj, name);
		fgw_async_error(ctx->obj, "\n");
		ST(0) = newSVpvs("0");
	}
	else
		ST(0) = newSVpvs("1");

	sv_2mortal(ST(0));
	return XSRETURN(1);
}


extern void boot_DynaLoader(pTHX_ CV* cv);
static void xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;
	newXS("fgw_func_reg", fgws_perl_func_reg, __FILE__);
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

/* API: init the interpreter so that functions can be registered */
static int fgws_perl_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	perl_ctx_t *ctx = calloc(sizeof(perl_ctx_t), 1);

	/* Start up a perl interpreter */
	ctx->interp = perl_alloc();
	if (ctx->interp == NULL) {
		free(ctx);
		return -1;
	}

	PERL_SET_CONTEXT(ctx->interp);
	perl_construct(ctx->interp);
	obj->script_data = ctx;
	ctx->interp->perl_user_data = ctx;
	ctx->obj = obj;
	ctx->freg_delay = 1;

	return 0;
}

/* API: load a script into an object */
static int fgws_perl_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	perl_ctx_t *ctx = obj->script_data;
	char *tmp[2];
	long n;

	tmp[0] = NULL;
	tmp[1] = fgw_strdup(filename);

	/* Load the script */
	PERL_SET_CONTEXT(ctx->interp);
	perl_parse(ctx->interp, xs_init, 2, tmp, NULL);

	/* execute delayed command registrations */
	for(n = 0; n < ctx->fregs_used; n++) {
		newXS(ctx->fregs[n], fgws_perl_call_fgw, __FILE__);
		free(ctx->fregs[n]);
	}
	free(ctx->fregs);
	ctx->fregs = NULL;
	ctx->fregs_used = ctx->fregs_alloced = 0;
	ctx->freg_delay = 0;

	perl_run(ctx->interp);

	free(tmp[1]);
	return 0;
}

static int fgws_perl_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".pl", ".perl", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_perl_eng = {
	"perl",
	fgws_perl_call_script,
	fgws_perl_init,
	fgws_perl_load,
	fgws_perl_unload,
	fgws_perl_reg_func,
	NULL,
	fgws_perl_test_parse
};

int pplg_check_ver_fungw_perl(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_perl(void)
{
	fgw_eng_reg(&fgw_perl_eng);
	return 0;
}

void pplg_uninit_fungw_perl(void)
{
	fgw_eng_unreg(fgw_perl_eng.name);
}
