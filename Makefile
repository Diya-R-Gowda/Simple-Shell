CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -MMD -MP
TARGET = myshell
SOURCES = src/main.c src/parser.c src/builtins.c src/executor.c src/environment.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

test: $(TARGET)
	bash tests/test_cases.sh

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)

-include $(OBJECTS:.o=.d)

.PHONY: all clean test
