CFLAGS=-g -Wall -std=gnu99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -fPIC
CFLAGS2=-g -Wall -std=gnu99 -c
TEST_CFLAGS=-Wl,-rpath=.
TEST_LDFLAGS=-ldl

all: hw3lib.so mm_test

hw3lib.so: mm_alloc.o
	gcc -shared -o $@ $^

mm_alloc.o: mm_alloc.c
	gcc $(CFLAGS) -c -o $@ $^

mm_test: mm_test.c
	gcc $(CFLAGS) $(TEST_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

%.o:%.c
	gcc $(CFLAGS2) -o $@ $<

malloc_test: mm_alloc.o malloc_test.o
	gcc $^ -o $@

clean:
	rm -rf hw3lib.so mm_alloc.o mm_test malloc_test malloc_test.o
