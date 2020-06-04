#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fgw_string.h"
#include <libfungwbind/c/fungw_c.h>

#include "static_lang.c"

char *def_script = NULL;
int ref_ucc;
extern int *atoi_ucc;

void async_error(fgw_obj_t *obj, const char *msg)
{
	printf("Async error: %s: %s\n", obj->name, msg);
}

/* call a funciton defined in an object (e.g. from lua) */
int hello(fgw_ctx_t *ctx, int how_many, char *who)
{
	fgw_arg_t res, arg[3];
	fgw_func_t *f;

	f = fgw_func_lookup(ctx, "hello");

	if (f != NULL) {
		arg[0].type = FGW_FUNC;
		arg[0].val.argv0.func = f;
		arg[0].val.argv0.user_call_ctx = &ref_ucc;

		arg[1].type = FGW_INT;
		arg[1].val.nat_int = how_many;

		arg[2].type = FGW_STR;
		arg[2].val.str = who;

		f->func(&res, 3, arg);

		if (res.type != FGW_INT) {
			if ((res.type != FGW_DOUBLE) && (res.type != FGW_LONG) && (res.type != FGW_STR) && (res.type != (FGW_STR | FGW_DYN)))
				printf("WARNING: returned non-int, non-double, non-str: %x\n", res.type);
			fgw_arg_conv(ctx, &res, FGW_INT);
		}
	}
	else {
		printf("ERROR: hello() not found.\n");
		return -1;
	}

	return res.val.nat_int;
}

/* Glue for C "scripting" - the code is static linked for this example; could load a .so in real code */
static int c_loader(fgw_obj_t *obj, const char *filename, const char *opts)
{
	if (strcmp(filename, "hello.c") != 0) {
		fprintf(stderr, "C \"script\" %s is not available\n", filename);
		exit(1);
	}
	hello_load(obj);
	return 0;
}

const char *lng;
char *script, script_buff[128];
void config()
{
	FILE *f;

	lng = getenv("LNG");
	script = getenv("SCRIPT");
	if (script == NULL) {
		fprintf(stderr, "Error: need SCRIPT set to the test script path\n");
		exit(1);
	}

	f = fopen(script, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: can not open %s for read\n", script);
		exit(1);
	}
	if ((lng == NULL) || (*lng == '\0')) {
		lng = fgw_engine_find(script, f);
		if (lng == NULL) {
			fprintf(stderr, "Error: can not figure the language of %s; please specify it in LNG\n", script);
			exit(1);
		}
	}
	fclose(f);
}

int main()
{
	fgw_ctx_t ctx;
	int res;

	/* initialize the "string" engine and language binding engines */
	string_init();
	static_lang_init();

/* glue for C "scripting" - always needs special treatment */
	fgw_c_eng.load = c_loader;
	fgw_eng_reg(&fgw_c_eng);

	config();

	printf("\n### Set up %s (%s)\n", lng, script);
	fflush(stdout);

	/* initialize a context */
	fgw_init(&ctx, "host");
	ctx.async_error = async_error;

	/* create objects (load script if needed) */
	fgw_obj_new(&ctx, "str", "string", NULL, NULL);
	fgw_obj_new(&ctx, "hello", lng, script, NULL);

	/* Call an object function using a local wrapper */
	printf("\n### Call hello()\n");
	fflush(stdout);
	res = hello(&ctx, 12, "blobbs");
	printf("res=%d\n", res);
	fflush(stdout);

	if (getenv("VERBOSE") != NULL) {
		printf("\n### Public functions:\n");
		fgw_dump_ctx(&ctx, stdout, "");
		fflush(stdout);
	}

	fgw_uninit(&ctx);
	fgw_atexit();

	printf("user call context: ");
	if (atoi_ucc == &ref_ucc)
		printf("ok\n");
	else if (atoi_ucc == NULL)
		printf("BROKEN! (NULL)\n");
	else
		printf("BROKEN! (wrong value)\n");

	return 0;
}
