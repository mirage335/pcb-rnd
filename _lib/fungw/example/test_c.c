#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fgw_string.h"
#include <libfungwbind/c/fungw_c.h>

int wrap_atoi(fgw_ctx_t *ctx, const char *s)
{
	fgw_arg_t res;
	fgw_vcall(ctx, &res, "atoi", FGW_STR, s, 0);
	fgw_arg_conv(ctx, &res, FGW_INT);
	return res.val.nat_int;
}

int main()
{
	fgw_ctx_t ctx;
	fgw_arg_t res, arg[8];
	fgw_error_t err;
	fgw_func_t *func_atoi;
	char *sres;

	/* initialize the "string" engine */
	string_init();

	/* initialize a context */
	fgw_init(&ctx, "host");

	/* Create an object called "str", using the "string" engine */
	fgw_obj_new(&ctx, "str", "string", NULL, NULL);

	/* get a function handle */
	func_atoi = fgw_func_lookup(&ctx, "atoi");
	if (func_atoi == NULL) {
		fprintf(stderr, "Error: failed to find global function atoi\n");
		return 1;
	}

	/* direct C call to the C function - no fungw overhead, but type conversion overhead still applies; assumes the function is implemented in C */
	arg[0].type = FGW_FUNC;
	arg[0].val.func = func_atoi;
	arg[1].type = FGW_STR;
	arg[1].val.str = "42";
	err = fgwc_atoi(&res, 2, arg);
	printf("err=%d rt=%x rv=%d\n", err, res.type, res.val.nat_int);

	/* string based call */
	sres = fgw_scall(&ctx, "atoi", "42", NULL);
	printf("scall sres='%s'\n", sres);
	free(sres);

	/* type/data based call */
	err = fgw_vcall(&ctx, &res, "atoi", FGW_STR, "42", NULL);
	printf("vcall err=%d rt=%x rv=%d\n", err, res.type, res.val.nat_int);

	/* local wrapper */
	printf("wrap: %d\n", wrap_atoi(&ctx, "123"));

	/* clean up and exit */
	fgw_dump_ctx(&ctx, stdout, "");
	fgw_uninit(&ctx);
	fgw_atexit();
	return 0;
}
