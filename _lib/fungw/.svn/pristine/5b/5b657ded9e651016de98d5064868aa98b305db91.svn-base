#include <stdlib.h>
#include <libfungw/fungw.h>
#include <libfungwbind/c/fungw_c.h>

static const char *dom_string = "c-string";

fgw_error_t fgwc_malloc(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	FGW_DECL_CTX;
	FGW_ARGC_REQ_MATCH(1);
	FGW_ARG_CONV(&argv[1], FGW_SIZE_T);
	fgw_ptr_reg(ctx, res, dom_string, FGW_VOID, malloc(argv[0].val.nat_size_t));
	return FGW_SUCCESS;
}


fgw_error_t fgwc_free(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	FGW_DECL_CTX;
	FGW_ARGC_REQ_MATCH(1);
	FGW_ARG_CONV(&argv[1], FGW_PTR | FGW_VOID);
	res->type = FGW_VOID;
	fgw_ptr_unreg(ctx, &argv[1], dom_string);
	free(argv[1].val.ptr_void);
	return FGW_SUCCESS;
}

int *atoi_ucc = NULL;
fgw_error_t fgwc_atoi(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	FGW_DECL_CTX;
	FGW_ARGC_REQ_MATCH(1);
	FGW_ARG_CONV(&argv[1], FGW_STR);
	atoi_ucc = argv[0].val.argv0.user_call_ctx;
	res->val.nat_int = atoi(argv[1].val.str);
	res->type = FGW_INT;
	return FGW_SUCCESS;
}

static int on_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	fgw_func_reg(obj, "atoi",   fgwc_atoi);
	fgw_func_reg(obj, "malloc", fgwc_malloc);
	fgw_func_reg(obj, "free",   fgwc_free);
	return 0;
}


static const fgw_eng_t fgw_string_eng = {
	"string",
	fgws_c_call_script,
	NULL,
	on_load
};

void string_init(void)
{
	fgw_eng_reg(&fgw_string_eng);
}
