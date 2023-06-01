CC=gcc

CFLAGS=-c -Wall -g
LDFLAGS=

EXECUTABLE=cfcc

DEPENDENCIES_TREE_SITTER_C=deps/tree-sitter-c/src/parser.c
DEPENDENCIES_TREE_SITTER_API=deps/tree-sitter/lib/src/lib.c
DEPENDENCIES=$(DEPENDENCIES_TREE_SITTER_C:deps/tree-sitter-c/src/%.c=%.o) $(DEPENDENCIES_TREE_SITTER_API:deps/tree-sitter/lib/src/%.c=%.o)

SOURCES=$(wildcard src/*.c)

SRC_OBJECTS=$(addprefix bin/, $(SOURCES:src/%.c=%.o))
DEP_OBJECTS=$(addprefix bin/, $(DEPENDENCIES))
OBJECTS=$(SRC_OBJECTS) $(DEP_OBJECTS)

all: $(EXECUTABLE)
	mkdir -p bin

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) bin/main.o bin/lib.o bin/parser.o -o bin/$@

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -Ideps/tree-sitter-c/src -Ideps/tree-sitter/lib/include -Isrc -c $< -o $@

bin/%.o: deps/tree-sitter-c/src/%.c
	$(CC) $(CFLAGS) -Ideps/tree-sitter-c/src -Isrc -c $< -o $@

bin/%.o: deps/tree-sitter/lib/src/%.c
	$(CC) $(CFLAGS) -Ideps/tree-sitter/lib/include -Isrc -c $< -o $@

clean:
	rm -rf bin/*