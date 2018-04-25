CC=gcc
CFLAGS=-ggdb3 -c -Wall -std=gnu99
LDFLAGS=-pthread
SOURCES=httpserver.c libhttp.c wq.c thpool.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=httpserver
CFLAGS2=

WIN_EXECUTABLE=httpserver.exe
WIN_SOURCES=$(SOURCES) realpath.c
WIN_OBJECTS=$(WIN_SOURCES:.c=.o)

debug: CFLAGS += -DDEBUG -g -DDEBUG_THPOOL
debug: all

windows: CFLAGS+=-DWINDOWS
windows: CFLAGS2+=-lws2_32
windows: $(WIN_EXECUTABLE)

windows-debug: CFLAGS += -DWINDOWS -DDEBUG -g -DDEBUG_THPOOL
windows-debug: windows

all: $(EXECUTABLE)

$(WIN_EXECUTABLE): $(WIN_SOURCES) $(WIN_OBJECTS)
	$(CC) $(LDFLAGS) $(WIN_OBJECTS) -o $@ -lws2_32

$(EXECUTABLE): $(SOURCES) $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ $(CFLAGS2)


clean:
	rm -f $(EXECUTABLE) $(OBJECTS)

clean-windows:
	rm -f $(WIN_EXECUTABLE) $(WIN_OBJECTS)
