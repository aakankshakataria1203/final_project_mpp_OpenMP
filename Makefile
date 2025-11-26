CC=gcc
CFLAGS=-O3 -g -std=gnu11 -Wall -Wextra -fopenmp
LDFLAGS=-lm -fopenmp


SOURCES=task_scheduler.c workloads.c
OBJECTS=$(SOURCES:.c=.o)


all: benchmark test_correctness


benchmark: benchmark.o $(OBJECTS)
   $(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


test_correctness: test_correctness.o $(OBJECTS)
   $(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


%.o: %.c
   $(CC) $(CFLAGS) -c $< -o $@


clean:
   rm -f benchmark test_correctness *.o


.PHONY: all clean
