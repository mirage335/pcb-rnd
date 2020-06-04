/*
    fungw - language-agnostic function gateway
    Copyright (C) 2017, 2019  Tibor 'Igor2' Palinkas

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
#include <tcl.h>

static int fgws_tcl_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".tcl", NULL};
	return fgw_test_parse_fn(filename, exts);
}

static void fgws_tcl_err(fgw_obj_t *obj)
{
	Tcl_Obj *res;

	res = Tcl_GetObjResult(obj->script_data);
	fgw_async_error(obj, res->bytes);
}

/* API: the script is calling an fgw function */
static int fgws_tcl_call_fgw(ClientData dat, Tcl_Interp *interp, int argc, char *argv[])
{
	fgw_obj_t *obj = dat;
	fgw_arg_t res, *sarg, sarg_static[128];
	int n;
	fgw_func_t *func;
	fgw_error_t err;

	func = fgw_func_lookup(obj->parent, argv[0]);
	if (func == NULL)
		return 0;

	/* alloc arguments */
	if ((argc+1) > (sizeof(sarg_static) / sizeof(sarg_static[0])))
		sarg = malloc((argc+1) * sizeof(fgw_arg_t *));
	else
		sarg = sarg_static;

	/* set up arguments */
	sarg[0].type = FGW_FUNC;
	sarg[0].val.argv0.func = func;
	sarg[0].val.argv0.user_call_ctx = obj->script_user_call_ctx;

	for(n = 1; n < argc; n++) {
		sarg[n].type = FGW_STR;
		sarg[n].val.str = argv[n];
	}

	/* Run command */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc, sarg);
	/* no need to free argv - all static strings */

	if (sarg != sarg_static)
		free(sarg);

	Tcl_ResetResult(obj->script_data);

	if (err == 0) {
		fgw_arg_conv(obj->parent, &res, FGW_STR | FGW_DYN);
		Tcl_AppendResult(obj->script_data, res.val.str, NULL);
		free(res.val.str);
	}

	return err ? -1 : 0;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_tcl_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	Tcl_CreateCommand(obj->script_data, name, (Tcl_CmdProc *)fgws_tcl_call_fgw, (ClientData)obj, (Tcl_CmdDeleteProc *)NULL);
}

/* API: fgw calls a tcl function */
static fgw_error_t fgws_tcl_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	int n, evr;
	fgw_error_t ret = FGW_SUCCESS;
	char *s;
	const char **sarg, *sarg_static[128];

	if ((argc+1) > (sizeof(sarg_static) / sizeof(sarg_static[0])))
		sarg = malloc((argc+1) * sizeof(char *));
	else
		sarg = sarg_static;

	sarg[0] = argv[0].val.func->name;
	for(n = 1; n < argc; n++) {
		fgw_arg_conv(obj->parent, &argv[n], FGW_STR);
		sarg[n] = argv[n].val.str;
	}

	s = Tcl_Merge(argc, sarg);

	fgws_ucc_save(obj);
	evr = Tcl_Eval(obj->script_data, s);
	fgws_ucc_restore(obj);

	if (evr != TCL_OK) {
		fgws_tcl_err(obj);
		ret = FGW_ERR_UNKNOWN;
	}
	Tcl_Free(s);

	res->type = FGW_STR;
	res->val.str = (char *)Tcl_GetStringResult(obj->script_data);

	fgw_argv_free(obj->parent, argc, argv);
	if (sarg != sarg_static)
		free(sarg);

	return ret;
}

/* API: unload the script */
static int fgws_tcl_unload(fgw_obj_t *obj)
{
	Tcl_DeleteInterp(obj->script_data);
	return 0;
}

/* Helper function for the script to register its functions */
static int fgws_tcl_freg(ClientData dat, Tcl_Interp *interp, int argc, char *argv[])
{
	fgw_obj_t *obj = dat;
	fgw_func_t *func;

	if (argc != 2) {
		fgw_async_error(obj, "fgw_func_reg: wrong number of arguments: need 1\n");
		return -1;
	}

	func = fgw_func_reg(obj, argv[1], fgws_tcl_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_func_reg: failed to register function\n");
		fgw_async_error(obj, argv[1]);
		fgw_async_error(obj, "\n");
		return -1;
	}

	return 0;
}

/* API: init the interpreter so that functions can be registered */
static int fgws_tcl_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	Tcl_FindExecutable("");

	obj->script_data = Tcl_CreateInterp();

	if (Tcl_Init(obj->script_data) == TCL_ERROR) {
		fgw_async_error(obj, "fgws_tcl_init: failed to create an interpreter\n");
		return -1;
	}

	Tcl_CreateCommand(obj->script_data, "fgw_func_reg", (Tcl_CmdProc *)fgws_tcl_freg, (ClientData)obj, (Tcl_CmdDeleteProc *)NULL);

	return 0;
}

/* API: load a script into an object */
static int fgws_tcl_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	/* Read the file */
	if (Tcl_EvalFile(obj->script_data, filename) == TCL_OK)
		return 0;

	fgw_async_error(obj, "fgws_tcl_load: failed to eval the file\n");
	fgws_tcl_err(obj);
	return -1;
}

/* API: engine registration */
static const fgw_eng_t fgw_tcl_eng = {
	"tcl",
	fgws_tcl_call_script,
	fgws_tcl_init,
	fgws_tcl_load,
	fgws_tcl_unload,
	fgws_tcl_reg_func,
	NULL,
	fgws_tcl_test_parse
};

int pplg_check_ver_fungw_tcl(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_tcl(void)
{
	fgw_eng_reg(&fgw_tcl_eng);
	return 0;
}

void pplg_uninit_fungw_tcl(void)
{
	fgw_eng_unreg(fgw_tcl_eng.name);
}
