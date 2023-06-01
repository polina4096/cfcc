CC=gcc

CFLAGS=-c -Wall -g
LDFLAGS=

EXECUTABLE=cfcc

DEPENDENCIES=tree-sitter-c/src/parser.c
SOURCES=$(wildcard src/*.c)

SRC_OBJECTS=$(addprefix bin/, $(SOURCES:src/%.c=%.o))
DEP_OBJECTS=$(addprefix bin/, $(DEPENDENCIES:tree-sitter-c/src/%.c=%.o))
OBJECTS=$(SRC_OBJECTS) $(DEP_OBJECTS)

all: $(EXECUTABLE)
	mkdir -p bin

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) bin/main.o -o bin/$@

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -Itree-sitter-c/src -Isrc -c $< -o $@

bin/%.o: tree-sitter-c/src/%.c
	$(CC) $(CFLAGS) -Itree-sitter-c/src -Isrc -c $< -o $@

clean:
	rm -rf bin/*