#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fgw_string.h"
#include <libfungwbind/c/fungw_c.h>

int main()
{
	fgw_ctx_t ctx;
	char line[1024];
	fgw_obj_t *obj;

	string_init();

	fgw_init(&ctx, "host");
	obj = fgw_obj_new(&ctx, "str", "string", NULL, NULL);

	fgw_dump_ctx(&ctx, stdout, "");

	while(fgets(line, sizeof(line), stdin) != NULL) {
		char *fname = line, *args, *next;
		fgw_func_t *fnc;
		int fargc;
		fgw_arg_t fargv[32], res;
		
		while(isspace(*fname)) fname++;
		if ((*fname == '#') || (*fname == '\r') || (*fname == '\n') || (*fname == '\0'))
			continue;
		args = strpbrk(fname, "( \t\r\n");
		if (args != NULL) {
			*args = '\0';
			args++;
		}
		fnc = fgw_func_lookup(&ctx, fname);
		if (fnc == NULL) {
			printf("ERROR: no such function\n");
			continue;
		}

		fargv[0].type = FGW_FUNC;
		fargv[0].val.func = fnc;
		for(fargc = 1; args != NULL; fargc++, args = next) {
			next = strchr(args, ',');
			if (next != NULL) {
				*next = '\0';
				next++;
			}
			while(isspace(*args)) args++;
			fargv[fargc].type = FGW_STR;
			fargv[fargc].val.str = args;
		}

		printf("call %s %p %d\n", fname, fnc, fnc->func(&res, fargc, fargv));
		fgw_arg_conv(&ctx, &res, FGW_STR);
		printf("  res=%s\n", res.val.str);
		fgw_arg_free(&ctx, &res); /* the above convert may have allocated a string so it is an FGW_DYN now */

	}

	fgw_uninit(&ctx);
	fgw_atexit();
	return 0;
}
