CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
TARGET = myshell
SOURCES = src/main.c src/parser.c src/builtins.c src/executor.c src/environment.c
OBJECTS = $(SOURCES:.c=.o)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
