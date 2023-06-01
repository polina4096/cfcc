#ifndef CFCC_HIR_C
#define CFCC_HIR_C

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.c"

// Common types
struct Variable {
    const char* name;
    struct Type* type;
};

struct Function {
    char* identifier;
    
    struct Variable* params;
    size_t params_length;

    struct Type* return_type;

    struct Variable* locals;
    size_t locals_length;

    struct Statement* body;
    size_t body_length;
};

// Compilation Unit
struct Unit {
    struct Function* functions;
    size_t functions_length;
    
    struct Variable* globals;
    size_t globals_length;
};

// Statements
struct StmtReturn {
    struct Expression* expr;
};

enum StatementKind {
    STMT_RETURN
};

struct Statement {
    enum StatementKind kind;
    union {
        struct StmtReturn stmt_return;
    };
};

// Expressions
struct ExprVariable {
    struct Variable* variable;
};

struct ExprLiteral {
    char* value;
    struct Type* type;
};

struct ExprCall {
    struct Function* func;

    struct Expression* args;
    size_t args_length;
};

enum BinaryOperation {
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_DIV,
    BINARY_OP_MUL,
};

struct ExprBinaryOp {
    enum BinaryOperation kind;
    struct Expression* left;
    struct Expression* right;
};

enum ExpressionKind {
    EXPR_VARIABLE,
    EXPR_LITERAL,
    EXPR_CALL,
    EXPR_BIN_OP,
};

struct Expression {
    enum ExpressionKind kind;
    union {
        struct ExprVariable expr_variable;
        struct ExprLiteral expr_literal;
        struct ExprCall expr_call;
        struct ExprBinaryOp expr_binary_op;
    };
};

// Lowering
#include "../deps/tree-sitter-c/src/tree_sitter/parser.h"
#include "../deps/tree-sitter-c/src/parser.c"
#include "../deps/tree-sitter/lib/include/tree_sitter/api.h"

// Utils
void print_nodes(TSNode node) {
    size_t children_count = ts_node_child_count(node);
    for (int i = 0; i < children_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);
        printf("%s\n", child_type);
    }
}

void print_named_nodes(TSNode node) {
    size_t children_count = ts_node_named_child_count(node);
    for (int i = 0; i < children_count; i++) {
        TSNode child = ts_node_named_child(node, i);
        const char* child_type = ts_node_type(child);
        printf("%s\n", child_type);
    }
}

void dbg_node(TSNode node) {
#ifndef DBG
    return;
#endif

    printf("%i %s\n", ts_node_symbol(node), ts_node_type(node));
    if (ts_node_child_count(node) > 0) {
        printf("\t- children -\n");
        print_nodes(node);
    }
}

void dbg_node_named(TSNode node) {
#ifndef DBG
    return;
#endif

    printf("%i %s\n\t- children -\n", ts_node_symbol(node), ts_node_type(node));
    if (ts_node_named_child_count(node) > 0) {
        printf("\t- children -\n");
        print_named_nodes(node);
    }
}

char* tsnstr(const char* src, TSNode node) {
    size_t start = ts_node_start_byte(node);
    size_t end = ts_node_end_byte(node);
    size_t length = end - start;

    char* buffer = malloc(length);
    memcpy(buffer, &src[start], length);
    buffer[length] = '\0';

    return buffer;
}

enum BinaryOperation parse_binary_op(char* str) {
    if (strcmp(str, "+") == 0)
        return BINARY_OP_ADD;

    else if (strcmp(str, "-") == 0)
        return BINARY_OP_SUB;

    else if (strcmp(str, "*") == 0)
        return BINARY_OP_MUL;
    
    else if (strcmp(str, "/") == 0)
        return BINARY_OP_DIV;

    else
        return -1;
}

// ast -> hir
void lower_expression(struct Expression* expr, const char* src, TSNode node) {
    dbg_node(node);

    switch (ts_node_symbol(node)) {
        case sym_binary_expression: {
            expr->kind = EXPR_BIN_OP;

            // 0  1  2
            // l  m  r
            // a  +  b
            TSNode  left_node = ts_node_child(node, 0);
            TSNode   mid_node = ts_node_child(node, 1);
            TSNode right_node = ts_node_child(node, 2);
            enum BinaryOperation op = parse_binary_op(tsnstr(src, mid_node));
            expr->expr_binary_op.kind = op;

            expr->expr_binary_op.left = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_binary_op.left, src, left_node);

            expr->expr_binary_op.right = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_binary_op.right, src, right_node);
            break;
        }

        case sym_number_literal: {
            expr->kind = EXPR_LITERAL;

            char* str = tsnstr(src, node);
            expr->expr_literal.value = str;

            // TODO: implement other number literals
            if (strstr(str, ".") != NULL) {
                // float
                expr->expr_literal.type = malloc(sizeof(struct Type));
                expr->expr_literal.type->kind = TYPE_KIND_BASIC;
                expr->expr_literal.type->basic = TYPE_F32;
            } else {
                // int
                expr->expr_literal.type = malloc(sizeof(struct Type));
                expr->expr_literal.type->kind = TYPE_KIND_BASIC;
                expr->expr_literal.type->basic = TYPE_I32;
            }

            break;
        }
    }
}

void lower_unit(struct Unit* unit, const char* src) {
    // unit->functions_length = 0;
    // unit->globals_length = 0;

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());

    TSTree* tree = ts_parser_parse_string(
        parser,
        NULL,
        src,
        strlen(src)
    );

    TSNode root_node = ts_tree_root_node(tree);
    size_t root_node_children_length = ts_node_named_child_count(root_node);
    for (int i = 0; i < root_node_children_length; i++) {
        TSNode node = ts_node_named_child(root_node, i);

        switch (ts_node_symbol(node)) {
            case sym_function_definition: {
                TSNode func_return_type_node = ts_node_named_child(node, 0);
                TSNode func_declarator_node = ts_node_named_child(node, 1);
                TSNode func_body_node = ts_node_named_child(node, 2);
                
                struct Type* type = malloc(sizeof(struct Type));
                lower_type(src, func_return_type_node, type);

                unit->functions_length += 1;
                unit->functions = realloc(unit->functions, sizeof(struct Function) * unit->functions_length);
                struct Function* func = &unit->functions[unit->functions_length - 1];
                func->return_type = type;

                TSNode func_identifier_node = ts_node_named_child(func_declarator_node, 0);
                func->identifier = tsnstr(src, func_identifier_node);

                // TSNode func_params_node = ts_node_named_child(func_declarator_node, 1);
                // size_t func_params_count = ts_node_named_child_count(func_params_node);
                // for (int j = 0; i < func_params_count; i++) {
                //     // TSNode func_param = ts_node_named_child(func_params_node, j);
                //     // print_named_nodes(func_param);
                //     // printf("\n");
                // }
                
                size_t func_body_node_children_length = ts_node_named_child_count(func_body_node);
                for (int j = 0; j < func_body_node_children_length; j++) {
                    TSNode stmt_node = ts_node_named_child(func_body_node, j);
                    switch (ts_node_symbol(stmt_node)) {
                        case sym_return_statement: {
                            func->body_length += 1;
                            func->body = realloc(func->body, sizeof(struct Statement) * func->body_length);
                            struct Statement* stmt = &func->body[func->body_length - 1];
                            stmt->kind = STMT_RETURN;
                            stmt->stmt_return.expr = malloc(sizeof(struct Expression));
                            
                            struct Expression* expr = stmt->stmt_return.expr;
                            TSNode expr_node = ts_node_named_child(stmt_node, 0);
                            lower_expression(expr, src, expr_node);
                            break;
                        }
                    }
                }

                break;
            }
        }
    }

#ifdef DBG
    printf("------------------------\n");
#endif

    // ts_tree_delete(tree);
    // ts_parser_delete(parser);
}

#endif