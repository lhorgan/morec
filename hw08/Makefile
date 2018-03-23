
HDRS := $(wildcard *.h)
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

CFLAGS := -g -O2 -std=c99
LDLIBS := -lm -lpthread

TIME := /usr/bin/time -p

main: $(OBJS)

%.c : %.o $(HDRS)

clean:
	rm -f *.o main *.out

test:
	@perl test.t

valgrind: main
	valgrind --leak-check=full ./main 1 4294967296 1024

clang-check:
	clang-check *.c --

time1: main
	$(TIME) ./main 1 1844674407370955161 40

time8: main
	$(TIME) ./main 8 1844674407370955161 40

time_big: main
	$(TIME) ./main 8 18446744073709551616 40

.PHONY: clean test valgrind clang-check time1 time8 time_big
