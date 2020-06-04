# --- configuration part --

# - generic configuration -
#  where scconfig source is; this is a path to a partial or full checkout of
#  svn://repo.hu/scconfig/trunk/src
SRC=src/

#  where compiled binaries (e.g. objects) should land; should be the same as
#  $(SRC) the project has its own copy of scconfig embedded
BIN=src/

#  what cflags to use to compile scconfig
USER_CFLAGS = -Wall -g -DGENCALL

#  what ldflags to use to link scconfig
USER_LDFLAGS =

#  in case hooks.c needs to link to something local
USER_OBJS =

#  what to build - a ./configure
all: configure

# This line imports scconfig core and default tests
include $(SRC)/default/Makefile.plugin

#
# - PLUGINS -
#
#  Comment this line if you are not interested in c99 features
#include $(SRC)/c99/Makefile.plugin

#  Comment this line if you do not need script libs to be detected
include $(SRC)/scripts/Makefile.plugin

#  Comment this line if you do not need parser libs to be detected
#include $(SRC)/parser/Makefile.plugin

#  Comment this line if you do not need to detect parser generators
#include $(SRC)/parsgen/Makefile.plugin

#  Comment this line if you do not need math related libs
#include $(SRC)/math/Makefile.plugin

#  Comment this line if you do not need socket/networking
#include $(SRC)/socket/Makefile.plugin

#  Comment this line if you do not need user/password API detection
#include $(SRC)/userpass/Makefile.plugin

#  Comment this line if you do not need gui (X11, toolkits)
#include $(SRC)/gui/Makefile.plugin

#  Comment this line if you do not need software utility libs (glib)
#include $(SRC)/sul/Makefile.plugin

#  Uncomment this line if you need menus
#include $(SRC)/menulib/Makefile.plugin

#  Comment this line if you do not need generator (templating); conflicts with tmpasm
#include $(SRC)/generator/Makefile.plugin

#  Comment this line if you do not need tmpasm (templating); conflicts with generator
include $(SRC)/tmpasm/Makefile.plugin

# --- you shouldn't edit the lines below ---
OBJS = $(USER_OBJS) hooks.o $(DEFAULT_NOMAIN_OBJS) $(SCRIPT_OBJS) $(PARSER_OBJS) $(GENERATOR_OBJS) $(TMPASM_OBJS) $(C99_OBJS) $(PARSGEN_OBJS) $(MATH_OBJS) $(SOCKET_OBJS) $(USERPASS_OBJS) $(GUI_OBJS) $(SUL_OBJS)
CFLAGS = $(USER_CFLAGS) $(DEFAULT_CFLAGS) $(SCRIPT_CFLAGS) $(PARSER_CFLAGS) $(GENERATOR_CFLAGS) $(TMPASM_CFLAGS) $(C99_CFLAGS) $(PARSGEN_CFLAGS) $(MATH_CFLAGS) $(SOCKET_CFLAGS) $(USERPASS_CFLAGS) $(GUI_CFLAGS)  $(SUL_CFLAGS) $(MENULIB_CFLAGS) -I$(SRC)/default
LDFLAGS = $(USER_LDFLAGS) $(DEFAULT_LDFLAGS) $(SCRIPT_LDFLAGS) $(PARSER_LDFLAGS) $(GENERATOR_LDFLAGS) $(TMPASM_LDFLAGS) $(C99_LDFLAGS) $(PARSGEN_LDFLAGS) $(MATH_LDFLAGS) $(SOCKET_LDFLAGS) $(USERPASS_LDFLAGS) $(GUI_LDFLAGS) $(SUL_LDFLAGS) $(MENULIB_LDFLAGS)

all: configure sccbox cquote

configure: $(OBJS) $(DEFAULT_MAIN_OBJS)
	$(CC) -o configure $(OBJS) $(DEFAULT_MAIN_OBJS)

menuconfig: $(OBJS) $(MENULIB_OBJS)
	$(CC) -o configure $(OBJS) $(MENULIB_OBJS)

src/util/sccbox.o: src/util/sccbox.c
	$(CC) -c $(CFLAGS) -o src/util/sccbox.o src/util/sccbox.c

sccbox: src/util/sccbox.o
	$(CC) $(LDFLAGS) -o sccbox src/util/sccbox.o

src/util/cquote.o: src/util/cquote.c
	$(CC) -c $(CFLAGS) -o src/util/cquote.o src/util/cquote.c

cquote: src/util/cquote.o
	$(CC) $(LDFLAGS) -o cquote src/util/cquote.o

hooks.o: ../libfungw/scconfig_hooks.h ../libfungwbind/scconfig_hooks.h

clean:
	rm $(OBJS) $(DEFAULT_MAIN_OBJS) configure sccbox cquote

distclean: clean
	-rm config.cache config.log
