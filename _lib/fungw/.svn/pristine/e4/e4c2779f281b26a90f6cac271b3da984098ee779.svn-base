#include <libfungw/fungw.h>

/* call a funciton defined in an object */
int my_atoi(fgw_ctx_t *ctx, const char *str)
{
	fgw_arg_t res, arg[2];
	fgw_func_t *f;

	f = fgw_func_lookup(ctx, "atoi");

	if (f != NULL) {
		arg[0].type = FGW_FUNC;
		arg[0].val.func = f;

		arg[1].type = FGW_STR;
		arg[1].val.cstr = str;

		f->func(&res, 2, arg);

		if (res.type != FGW_INT) {
			fprintf(stderr, "ERROR: atoi() returned non-int\n");
			return 0;
		}
	}
	else {
		printf("ERROR: atoi() not found.\n");
		return -1;
	}

	return res.val.nat_int;
}


/* Public function; atoi() is provided by another objects through fungw */
static fgw_error_t c_hello(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_ctx_t *ctx = argv[0].val.func->obj->parent;

	if (argc != 3)
		return FGW_ERR_ARGC;
	if (fgw_arg_conv(ctx, &argv[1], FGW_INT) != 0)
		return FGW_ERR_ARG_CONV;
	if (fgw_arg_conv(ctx, &argv[2], FGW_STR) != 0)
		return FGW_ERR_ARG_CONV;

	printf("Hello %d %s\n", argv[1].val.nat_int, argv[2].val.str);
	printf("atoi: %d\n", my_atoi(ctx, "42"));

	res->val.nat_int = 66;
	res->type = FGW_INT;
	return FGW_SUCCESS;
}

void hello_load(fgw_obj_t *obj)
{
	printf("hello world from init %d\n", my_atoi(obj->parent, "33"));

	/* register hello(), to be called from other objects and the host application */
	fgw_func_reg(obj, "hello", c_hello);
}

