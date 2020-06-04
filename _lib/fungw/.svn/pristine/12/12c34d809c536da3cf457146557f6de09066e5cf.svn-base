all: multicall

CFLAGS = -fPIC -Wall -g -I.. -I../src_3rd
LIBS = ../src_3rd/genht/genht_std.a
LIB_FGW = ../libfungw/libfungw.a
# $(FUNGWBIND)/c/fungw_c.o
LDLIBS = -lm
# $(FUNGWBIND_LDLIBS)

multicall: multicall.o $(LIB_FGW) $(ENGINES) $(LIBS)

../src_3rd/genht/genht_std.a:
	cd ../src_3rd/genht && make genht_std.a

clean:
	-rm multicall multicall.o

distclean: clean

