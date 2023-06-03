#ifndef CFCC_TYPE_C
#define CFCC_TYPE_C

#include <stddef.h>
#include <string.h>

enum TypeKind {
    TYPE_KIND_BASIC,
    TYPE_KIND_COMPOUND,
    TYPE_KIND_ARRAY,
};

enum FundamentalType {
    TYPE_I32,
    TYPE_F32,
};

struct Type;

struct Field {
    const char* name;
    struct Type* type;
};

struct Compound {
    struct Field* fields;
    size_t fields_length;
};

struct Array {
    struct Type* type;
    size_t length;
};

struct Type {
    enum TypeKind kind;
    union {
        enum FundamentalType basic;
        struct Compound compound;
        struct Array array;
    };
};

size_t type_size(struct Type* type);

static size_t type_size_basic(enum FundamentalType fundamental) {
    switch (fundamental) {
        case TYPE_I32:
            return 4;

        case TYPE_F32:
            return 4;
    }
}

static size_t type_size_compound(struct Compound* compound) {
    // size_t byte_size;
    // for (int i = 0; i < compound.fields_length; i++) {
        // compound.fields[i].type
    // }
    // TODO:
    return -1;
}

static size_t type_size_array(struct Array* array) {
    return type_size(array->type) * array->length;
}

size_t type_size(struct Type* type) {
    switch (type->kind) {
        case TYPE_KIND_BASIC:
            return type_size_basic(type->basic);

        case TYPE_KIND_COMPOUND:
            return type_size_compound(&type->compound);

        case TYPE_KIND_ARRAY:
            return type_size_array(&type->array);
    }
}

#include "../deps/tree-sitter/lib/include/tree_sitter/api.h"

void lower_type(const char* buffer, TSNode node, struct Type* type) {
    size_t start = ts_node_start_byte(node);
    size_t end = ts_node_end_byte(node);
    size_t length = end - start;

    if (strncmp(&buffer[start], "int", length) == 0) {
        type->kind  = TYPE_KIND_BASIC;
        type->basic = TYPE_I32;
    } else if (strncmp(&buffer[start], "float", length) == 0) {
        type->kind  = TYPE_KIND_BASIC;
        type->basic = TYPE_F32;
    } else {

    }
}

#endif