#include <stdio.h>
#include <assert.h>
#include <libfungw/fungw.h>

/* Test that the conversion of an argv[] on the first invocation of a function
   does not affect the second invocation in an multicall */

fgw_error_t test_cb(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	FGW_DECL_CTX;
	printf(" test_cb 1: %x '%s'\n", argv[1].type, argv[1].val.str);
	assert(fgw_arg_conv(ctx, &argv[1], FGW_DOUBLE) == 0);
	printf(" test_cb 2: %x %f\n", argv[1].type, argv[1].val.nat_double);
	return FGW_SUCCESS;
}

int main()
{
	fgw_ctx_t ctx;
	fgw_obj_t *obj1, *obj2;
	fgw_arg_t tmp[2];

	fgw_init(&ctx, "host");
	obj1 = fgw_obj_reg(&ctx, "o1");
	obj2 = fgw_obj_reg(&ctx, "o2");

	fgw_func_reg(obj1, "test", test_cb);
	fgw_func_reg(obj2, "test", test_cb);

	printf("scall:\n");
	fgw_scall_all(&ctx, "test", "3.14");

	printf("vcall:\n");
	fgw_vcall_all(&ctx, "test", FGW_STR, "3.14");

	printf("call:\n");
	tmp[0].type = FGW_INVALID; /* will be filled in by fgw_call_all() */
	tmp[1].type = FGW_STR;
	tmp[1].val.str = "3.14";
	fgw_call_all(&ctx, "test", 2, tmp);

	fgw_uninit(&ctx);
	fgw_atexit();
	return 0;
}
