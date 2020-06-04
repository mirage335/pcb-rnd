/*
    fungw - function gateway
    Copyright (C) 2017 Tibor 'Igor2' Palinkas

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* These functions can be called from an scconfig hooks.c to get libfungw
   bindings configured without having to have or compile or run a separate
   scconfig process for the lib. Assumes libfungw/scconfig_hhoks.h functions
   has been already called. */

#include <stdio.h>
#include <string.h>
#include "arg.h"
#include "log.h"
#include "dep.h"
#include "db.h"
#include "tmpasm_scconfig.h"

static int fungwbind_hook_postinit(void)
{
	db_mkdir("/local");
	put("/local/fungw/bindings_all", "fawk lua mawk tcl python python3 mruby perl duktape estutter funlisp cli");
	put("/local/fungw/bindings", "");
	return 0;
}

static int fungwbind_hook_detect_target(void)
{
	int static_disable_python = 0;
	put("libs/script/fawk/presents", strue);
	require("libs/script/lua/*", 0, 0);
	require("libs/script/mawk/*", 0, 0);
	require("libs/script/tcl/*", 0, 0);
	if (require("libs/script/python/*", 0, 0) == 0)
		static_disable_python = 3;
	if (require("libs/script/python3/*", 0, 0) == 0)
		static_disable_python = 2;
	require("libs/script/mruby/*", 0, 0);
	require("libs/script/perl/*", 0, 0);
	require("libs/script/duktape/*", 0, 0);
	require("libs/script/estutter/*", 0, 0);
	require("libs/script/funlisp/*", 0, 0);

	/* for cli */
	require("libs/proc/fork/*", 0, 0);
	require("libs/io/pipe/*", 0, 0);

	if (static_disable_python != 0) {
		char path[64];
		if (static_disable_python == 2)
			strcpy(path, "/local/fungw/binding/static-disable/python");
		else
			sprintf(path, "/local/fungw/binding/static-disable/python%d", static_disable_python);
		put(path, strue);
	}

	return 0;
}

#define gen_lang(langdir) \
	fprintf(stderr, "Generating libfungwbind/" langdir "/Makefile (%d)\n", generr |= tmpasm(root, langdir "/Plug.tmpasm", langdir "/Makefile"));

/* Root is the path for the fungwlug lib dir (trunk/libfungwbind) */
static int fungwbind_hook_generate(const char *root)
{
	int generr = 0;

	gen_lang("fawk");
	gen_lang("lua");
	gen_lang("mawk");
	gen_lang("tcl");
	gen_lang("python");
	gen_lang("python3");
	gen_lang("duktape");
	gen_lang("mruby");
	gen_lang("perl");
	gen_lang("estutter");
	gen_lang("funlisp");
	gen_lang("cli");

	fprintf(stderr, "Generating libfungwbind/Makefile (%d)\n", generr |= tmpasm(root, "Makefile.in", "Makefile"));
	fprintf(stderr, "Generating libfungwbind/libfungwbind.mak (%d)\n", generr |= tmpasm(root, "libfungwbind.mak.in", "libfungwbind.mak"));

	return generr;
}
