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
#include <libfungw/fungw.h>
#include <libfungw/fungw_conv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define LUA_OBJ_NAME "__fgw_obj__"

/* Extract the fgw object from a lua state (stored there as a global variable) */
static fgw_obj_t *fgws_lua_get_obj(lua_State *lst)
{
	lua_getglobal(lst, LUA_OBJ_NAME);
	return lua_touserdata(lst, -1);
}


/* Convert fgw type to lua type and push */
static void fgw_lua_push(fgw_ctx_t *fctx, lua_State *lst, fgw_arg_t *arg)
{
#	define FGW_LUA_PUSH_LONG(lst, val)   lua_pushinteger(lst, val); return;
#	define FGW_LUA_PUSH_DOUBLE(lst, val) lua_pushnumber(lst, val); return;
#	define FGW_LUA_PUSH_PTR(lst, val)    lua_pushlightuserdata(lst, val); return;
#	define FGW_LUA_PUSH_STR(lst, val)    lua_pushstring(lst, val); return;
#	define FGW_LUA_PUSH_NIL(lst, val)    lua_pushnil(lst); return;

	if (FGW_IS_TYPE_CUSTOM(arg->type))
		fgw_arg_conv(fctx, arg, FGW_AUTO);

	switch(FGW_BASE_TYPE(arg->type)) {
		ARG_CONV_CASE_LONG(lst, FGW_LUA_PUSH_LONG);
		ARG_CONV_CASE_LLONG(lst, FGW_LUA_PUSH_DOUBLE);
		ARG_CONV_CASE_DOUBLE(lst, FGW_LUA_PUSH_DOUBLE);
		ARG_CONV_CASE_LDOUBLE(lst, FGW_LUA_PUSH_DOUBLE);
		ARG_CONV_CASE_PTR(lst, FGW_LUA_PUSH_PTR);
		ARG_CONV_CASE_STR(lst, FGW_LUA_PUSH_STR);
		ARG_CONV_CASE_CLASS(lst, FGW_LUA_PUSH_NIL);
		ARG_CONV_CASE_INVALID(lst, FGW_LUA_PUSH_NIL);
	}
	if (arg->type & FGW_PTR) {
		FGW_LUA_PUSH_PTR(lst, arg->val.ptr_void);
	}
	else {
		FGW_LUA_PUSH_NIL(lst, 0);
	}
}

/* Read the lua stack and convert the result to an fgw arg */
static void fgw_lua_toarg(lua_State *lst, fgw_arg_t *dst, int n)
{
	switch(lua_type(lst, n)) {
		case LUA_TNUMBER:
			dst->type = FGW_DOUBLE;
			dst->val.nat_double = lua_tonumber(lst, n);
			break;
		case LUA_TBOOLEAN:
			dst->type = FGW_INT;
			dst->val.nat_int = !!lua_toboolean(lst, n);
			break;
		case LUA_TSTRING:
			dst->type = FGW_STR | FGW_DYN;
			dst->val.str = fgw_strdup(lua_tostring(lst, n));
			break;
		case LUA_TLIGHTUSERDATA:
			dst->type = FGW_PTR;
			dst->val.ptr_void = lua_touserdata(lst, n);
			break;
		case LUA_TNIL:
		default:
			dst->type = FGW_PTR;
			dst->val.ptr_void = NULL;
			break;
	}
}

/* API: the script is calling an fgw function */
static int fgws_lua_call_fgw(lua_State *lst)
{
	fgw_obj_t *obj;
	int argc, n;
	fgw_arg_t res, *argv, argv_static[16];
	fgw_func_t *func;
	fgw_error_t err;
	lua_Debug ar;

	/* Get the current function-name */
	lua_getstack(lst, 0, &ar);
	lua_getinfo(lst, "n", &ar);

	obj = fgws_lua_get_obj(lst);

	func = fgw_func_lookup(obj->parent, ar.name);
	if (func == NULL)
		return 0;

	argc = lua_gettop(lst);

	if ((argc + 1) > (sizeof(argv_static) / sizeof(argv_static[0])))
		argv = malloc((argc + 1) * sizeof(fgw_arg_t));
	else
		argv = argv_static;

	argv[0].type = FGW_FUNC;
	argv[0].val.argv0.func = func;
	argv[0].val.argv0.user_call_ctx = obj->script_user_call_ctx;

	for (n = 1; n < argc; n++)
		fgw_lua_toarg(lst, &argv[n], n);

	/* Run command */
	res.type = FGW_PTR;
	res.val.ptr_void = NULL;
	err = func->func(&res, argc, argv);

	/* Free the array */
	fgw_argv_free(obj->parent, argc, argv);
	if (argv != argv_static)
		free(argv);

	if (err != 0)
		return 0;

	fgw_lua_push(obj->parent, lst, &res);

	if (res.type & FGW_DYN)
		free(res.val.ptr_void);

	return 1;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_lua_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	lua_State *lst = obj->script_data;
	lua_register(lst, name, fgws_lua_call_fgw);
}

/* API: fgw calls a lua function */
static fgw_error_t fgws_lua_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	lua_State *lst = obj->script_data;
	int i;

	lua_getglobal(lst, argv[0].val.func->name);

	for (i = 1; i < argc; i++)
		fgw_lua_push(obj->parent, lst, &argv[i]);

	fgws_ucc_save(obj);
	lua_call(lst, argc-1, 1);
	fgws_ucc_restore(obj);

	fgw_lua_toarg(lst, res, 1);
	lua_pop(lst, 1);
	return FGW_SUCCESS;
}


/* API: unload the script */
static int fgws_lua_unload(fgw_obj_t *obj)
{
	if (obj->script_data != NULL)
		lua_close(obj->script_data);
	obj->script_data = NULL;
	return 0;
}

/* Helper function for the script to register its functions */
static int fgws_lua_freg(lua_State *lst)
{
	fgw_obj_t *obj;
	const char *name;
	int argc;
	fgw_func_t *func;

	obj = fgws_lua_get_obj(lst);

	argc = lua_gettop(lst);
	if (argc != 2) {
		fgw_async_error(obj, "fgw_func_reg: wrong number of arguments: need 2\n");
		return 0;
	}

	name = lua_tostring(lst, 1);
	if (name == NULL) {
		fgw_async_error(obj, "fgw_func_reg: empty name\n");
		return 0;
	}

	func = fgw_func_reg(obj, name, fgws_lua_call_script);
	if (func == NULL) {
		fgw_async_error(obj, "fgw_func_reg: failed to register function\n");
		fgw_async_error(obj, name);
		fgw_async_error(obj, "\n");
		return 0;
	}

	return 1;
}

/* API: init the interpreter so that functions can be registered */
static int fgws_lua_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	lua_State *lst;

	/* initialize the interpreter */
	obj->script_data = lst = luaL_newstate();

	if (lst == NULL) {
		fgw_async_error(obj, "fgws_lua_init: failed to set up the interpreter\n");
		return -1;
	}

	/* Load default libs */
#if LUA_VERSION_NUM < 500
	luaopen_base(lst);
	luaopen_io(lst);
	luaopen_string(lst);
	luaopen_math(lst);
#else
	luaL_openlibs(lst);
#endif

	/* add the lua->fgw glue */
	lua_register(lst, "fgw_func_reg", fgws_lua_freg);
	lua_pushlightuserdata(lst, obj);
	lua_setglobal(lst, LUA_OBJ_NAME);
	return 0;
}

/* API: load a script into an object */
static int fgws_lua_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	lua_State *lst = obj->script_data;
	int res;

	/* Load the file */
	res = luaL_loadfile(lst, filename);

	if (res != 0) {
		fgw_async_error(obj, "fgws_lua_load: failed to load the script\n");
		lua_close(obj->script_data);
		obj->script_data = NULL;
		return -1;
	}

	/* Run the main part */
	if (lua_pcall(lst, 0, 0, 0) != 0) {
		fgw_async_error(obj, "fgws_lua_load: failed to execute the script\n");
		lua_close(obj->script_data);
		obj->script_data = NULL;
		return -2;
	}

	return 0;
}

static int fgws_lua_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".lua", NULL };
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_lua_eng = {
	"lua",
	fgws_lua_call_script,
	fgws_lua_init,
	fgws_lua_load,
	fgws_lua_unload,
	fgws_lua_reg_func,
	NULL,
	fgws_lua_test_parse
};

int pplg_check_ver_fungw_lua(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_lua(void)
{
	fgw_eng_reg(&fgw_lua_eng);
	return 0;
}

void pplg_uninit_fungw_lua(void)
{
	fgw_eng_unreg(fgw_lua_eng.name);
}
