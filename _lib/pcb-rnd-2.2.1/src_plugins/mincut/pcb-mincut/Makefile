CFLAGS = -g -Wall -I../../../src_3rd -I../../../src -I../../..
LDFLAGS = -lm
OBJS = main.o solve.o load.o graph.o ../../../src_3rd/genht/htsi.o ../../../src_3rd/genht/hash.o

all: main

main: $(OBJS)

test:
	cd test_cases && make


clean:
	rm $(OBJS) 2>/dev/null ; true
