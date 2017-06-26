SRCS=shell.c tokenizer.c get_full_path.c
EXECUTABLES=shell

CC=gcc
CFLAGS=-g -Wall -std=gnu99 # -D DEBUG
LDFLAGS=

OBJS=$(SRCS:.c=.o)

all: $(EXECUTABLES)

$(EXECUTABLES): $(OBJS)
	echo $@ $^
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

.c.o:
	echo Objects = $(OBJS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLES) $(OBJS)
