
# Normal compilation options for developping
#CFLAGS= -DNDEBUG -g -fshort-enums -Wall -Wno-char-subscripts
#CFLAGS= -O3 -msse4.1 -fshort-enums -Wall -Wno-char-subscripts

# Normal compilation options for production
CFLAGS= -DNDEBUG -O3 -msse4.1 -fshort-enums -Wall -Wno-char-subscripts

# Compilation options for running valgrind
#CFLAGS=-O0 -g -Wall -std=gnu99

# Compilation options for profiling with gprof
#CFLAGS=-pg -O3 -DNDEBUG -msse4.1 -fshort-enums -Wall -Wno-char-subscripts

OBJS=patterns.o debug.o main.o
BIN=michi

all: $(BIN)

michi: $(OBJS) michi.o michi.h
	gcc $(CFLAGS) -std=gnu99 -o michi michi.o $(OBJS) -lm

%.o: %.c michi.h
	gcc $(CFLAGS) -c -std=gnu99 $<

test:
	tests/run

valgrind: michi
	valgrind --track-origins=yes ./michi tsdebug

callgrind: michi
	valgrind --tool=callgrind ./michi mcbenchmark

tags: $(BIN)
	ctags *.c *.h

clean:
	rm -f $(OBJS) michi.o michi-debug.o

veryclean: clean
	rm -f $(BIN)
	rm -rf tests/output tests/michi.log
