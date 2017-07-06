SRCS=shell.c tokenizer.c get_full_path.c jobs.c
EXECUTABLES=shell

CC=gcc
CFLAGS=-g -Wall -std=gnu99  -D DEBUG #-D DEBUG_GET_JOB_VECTOR
LDFLAGS=

OBJS=$(SRCS:.c=.o)

# test job.c
CFLAGS2=$(CFLAGS)
SRCS_JOB=test_jobs.c jobs.c get_full_path.c tokenizer.c
OBJS_JOB=$(SRCS_JOB:.c=.o)

all: $(EXECUTABLES)

$(EXECUTABLES): $(OBJS)
	echo $@ $^
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

.c.o:
	echo '$<' = $<
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLES) $(OBJS)

test_jobs: $(OBJS_JOB)
	$(CC) $(CFLAGS2) $(OBJS_JOB) $(LDFLAGS) -o $@

clean_jobs:
	rm -rf test_jobs $(OBJS_JOB)
