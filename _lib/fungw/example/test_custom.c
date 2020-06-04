#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "fgw_string.h"
#include <libfungwbind/c/fungw_c.h>
#include <libfungw/fungw_conv.h>

fgw_type_t FGW_MM; /* millimeter type ID; store distance in mm in the long field */

char *mm_dup_str(long val)
{
	char *res = malloc(32);
	if (val > 1000l*1000l) sprintf(res, "%f km", (double)val/(1000.0*1000.0));
	else if (val > 1000) sprintf(res, "%f m", (double)val/1000.0);
	else sprintf(res, "%ld mm", val);
	return res;
}

char *mm_dup_str_nounit(long val)
{
	char *res = malloc(32);
	sprintf(res, "%ld", val);
	return res;
}

long mmtol(const char *s, const char **end)
{
	char *e;
	long res = strtol(s, &e, 10);
	if (*e != '\0') {
		while(isspace(*e)) e++;
		if (strcmp(e, "mm") == 0) { *end = e+2; }
		else if (strcmp(e, "km") == 0) { *end = e+2; res *= 1000l*1000l; }
		else if (strcmp(e, "m") == 0)  { *end = e+1; res *= 1000; }
		else if (strcmp(e, "dm") == 0) { *end = e+2; res *= 100; }
		else if (strcmp(e, "cm") == 0) { *end = e+2; res *= 10; }
		else *end = e;
	}
	else
		*end = e;
	return res;
}

#define conv_mmtol(dst, src) \
	do { \
		const char *end; \
		dst = mmtol(src, &end); \
		if (*end != '\0') \
			return -1; \
	} while(0)

int mm_arg_conv(fgw_ctx_t *ctx, fgw_arg_t *arg, fgw_type_t target)
{
	if (target == FGW_MM) { /* convert to mm */
		long tmp;
		switch(FGW_BASE_TYPE(arg->type)) {
			ARG_CONV_CASE_LONG(tmp, conv_assign)
			ARG_CONV_CASE_LLONG(tmp, conv_assign)
			ARG_CONV_CASE_DOUBLE(tmp, conv_assign)
			ARG_CONV_CASE_LDOUBLE(tmp, conv_assign)
			ARG_CONV_CASE_STR(tmp, conv_mmtol)
			ARG_CONV_CASE_PTR(tmp, conv_err)
			ARG_CONV_CASE_CLASS(tmp, conv_err)
			ARG_CONV_CASE_INVALID(tmp, conv_err)
		}
		arg->type = FGW_MM;
		arg->val.nat_long = tmp;
		return 0;
	}
	if (arg->type == FGW_MM) { /* convert from mm */
		long tmp = arg->val.nat_long;
		switch(target) {
			ARG_CONV_CASE_LONG(tmp, conv_rev_assign)
			ARG_CONV_CASE_LLONG(tmp, conv_rev_assign)
			ARG_CONV_CASE_DOUBLE(tmp, conv_rev_assign)
			ARG_CONV_CASE_LDOUBLE(tmp, conv_rev_assign)
			ARG_CONV_CASE_PTR(tmp, conv_err)
			ARG_CONV_CASE_CLASS(tmp, conv_err)
			ARG_CONV_CASE_INVALID(tmp, conv_err)
			case FGW_STR:
				arg->val.str = mm_dup_str(tmp);
				arg->type = FGW_STR | FGW_DYN;
				return 0;
		}
		arg->type = target;
		return 0;
	}
	fprintf(stderr, "Neither side of the conversion is mm\n");
	abort();
}

/* Lazy approach: convert only to/from string */
int mm_arg_conv_lazy(fgw_ctx_t *ctx, fgw_arg_t *arg, fgw_type_t target)
{
	if (target == FGW_MM) { /* convert to string and then to mm */
		long tmp;
		const char *end;
		if (fgw_arg_conv(ctx, arg, FGW_STR) != 0)
			return -1;
		tmp = mmtol(arg->val.str, &end);
		if (arg->type & FGW_DYN)
			free(arg->val.str);
		if (*end != '\0')
			return -1;
		arg->type = FGW_MM;
		arg->val.nat_long = tmp;
		return 0;
	}
	if (arg->type == FGW_MM) { /* convert from mm */
		long tmp = arg->val.nat_long;
		arg->val.str = mm_dup_str_nounit(tmp);
		arg->type = FGW_STR | FGW_DYN;
		return 0;
	}
	fprintf(stderr, "Neither side of the conversion is mm\n");
	abort();
}

void test(fgw_ctx_t *ctx)
{
	fgw_arg_t a, b;
	int res;

	a.val.str = "124cm";
	a.type = FGW_STR;

	res = fgw_arg_conv(ctx, &a, FGW_MM);
	printf("str to mm: %d: 0x%x %ld\n", res, a.type, a.val.nat_long);

	b = a;

	res = fgw_arg_conv(ctx, &a, FGW_STR);
	printf("mm to str: %d: 0x%x %s\n", res, a.type, a.val.str);

	res = fgw_arg_conv(ctx, &b, FGW_DOUBLE);
	printf("mm to double: %d: 0x%x %f\n", res, b.type, b.val.nat_double);
}

int main()
{
	fgw_ctx_t ctx;

	string_init();

	fgw_init(&ctx, "host");

	printf("--- proper:\n");
	FGW_MM = fgw_reg_custom_type(&ctx, 0, "mm", mm_arg_conv, NULL);
	assert(FGW_MM != FGW_INVALID);
	test(&ctx);

	printf("--- lazy:\n");
	FGW_MM = fgw_reg_custom_type(&ctx, 0, "mm", mm_arg_conv_lazy, NULL);
	assert(FGW_MM != FGW_INVALID);
	test(&ctx);

	fgw_uninit(&ctx);
	fgw_atexit();
	return 0;
}
