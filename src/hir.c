#ifndef CFCC_HIR_C
#define CFCC_HIR_C

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.c"

// Common types
struct Variable {
    const char* identifier;
    struct Type* type;
};

struct Function;

struct Scope {
    struct Scope* outer;

    struct Function** functions;
    size_t functions_length;
    
    struct Variable** variables;
    size_t variables_length;
};


struct Function {
    char* identifier;
    
    struct Variable** params;
    size_t params_length;

    struct Type* return_type;

    struct Scope scope;

    struct Statement** body;
    size_t body_length;
};

// Compilation Unit
struct Unit {
    struct Scope scope;
};

// Expressions
struct ExprVariable {
    struct Variable* variable;
};

// Expressions
struct ExprAssignment {
    struct Variable* variable;
    struct Expression* expression;
};

struct ExprLiteral {
    char* value;
    struct Type* type;
};

struct ExprCall {
    struct Function* func;

    struct Expression** args;
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
    EXPR_ASSIGNMENT,
    EXPR_LITERAL,
    EXPR_CALL,
    EXPR_BIN_OP,
};

struct Expression {
    enum ExpressionKind kind;
    union {
        struct ExprVariable expr_variable;
        struct ExprAssignment expr_assignment;
        struct ExprLiteral expr_literal;
        struct ExprCall expr_call;
        struct ExprBinaryOp expr_binary_op;
    };
};

// Statements
struct StmtReturn {
    struct Expression expr;
};

struct StmtExpression {
    struct Expression expr;
};

enum StatementKind {
    STMT_RETURN,
    STMT_EXPRESSION,
};

struct Statement {
    enum StatementKind kind;
    union {
        struct StmtReturn stmt_return;
        struct StmtExpression stmt_expression;
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

    printf("\n%i %s\n", ts_node_symbol(node), ts_node_type(node));
    if (ts_node_child_count(node) > 0) {
        printf("\t- children -\n");
        print_nodes(node);
    }
}

void dbg_node_named(TSNode node) {
#ifndef DBG
    return;
#endif

    printf("\n%i %s\n\t- children -\n", ts_node_symbol(node), ts_node_type(node));
    if (ts_node_named_child_count(node) > 0) {
        printf("\t- children -\n");
        print_named_nodes(node);
    }
}

char* tsnstr(const char* src, TSNode node) {
    size_t start = ts_node_start_byte(node);
    size_t end = ts_node_end_byte(node);
    size_t length = end - start;

    char* buffer = malloc(length + 1);
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

struct Function* find_func(char* identifier, struct Scope* scope) {
    for (int i = 0; i < scope->functions_length; i++) {
        if (strcmp(identifier, scope->functions[i]->identifier) == 0) {
            return scope->functions[i];
        }
    }

    if (scope->outer != NULL) {
        return find_func(identifier, scope->outer);
    }

    return NULL;
}

struct Variable* find_var(char* identifier, struct Scope* scope) {
    for (int i = 0; i < scope->variables_length; i++) {
        if (strcmp(identifier, scope->variables[i]->identifier) == 0) {
            return scope->variables[i];
        }
    }

    if (scope->outer != NULL) {
        return find_var(identifier, scope->outer);
    }

    return NULL;
}

// ast -> hir
void lower_expression(struct Expression* expr, struct Scope* scope, const char* src, TSNode node) {
    switch (ts_node_symbol(node)) {
        case sym_call_expression: {
            expr->kind = EXPR_CALL;
            
            TSNode ident_node = ts_node_named_child(node, 0);
            char* identifier = tsnstr(src, ident_node);
            expr->expr_call.func = find_func(identifier, scope);
            
            if (expr->expr_call.func == NULL) {
                printf("function `%s` not found\n", identifier);
            }

            // TODO: args
            // TSNode args_node = ts_node_named_child(node, 1);
            // dbg_node(args_node);

            break;
        }
        
        case sym_identifier: {
            expr->kind = EXPR_VARIABLE;

            char* identifier = tsnstr(src, node);
            expr->expr_variable.variable = find_var(identifier, scope);
            
            if (expr->expr_variable.variable == NULL) {
                printf("variable `%s` not found\n", identifier);
            }

            
            break;
        }

        case sym_assignment_expression: {
            expr->kind = EXPR_ASSIGNMENT;

            char* identifier = tsnstr(src, ts_node_named_child(node, 0));
            expr->expr_assignment.variable = find_var(identifier, scope);
            
            if (expr->expr_assignment.variable == NULL) {
                printf("variable `%s` not found\n", identifier);
            }

            TSNode value_node = ts_node_named_child(node, 1);
            expr->expr_assignment.expression = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_assignment.expression, scope, src, value_node);
            
            break;
        }

        case sym_parenthesized_expression: {
            TSNode inner_node = ts_node_named_child(node, 0);
            lower_expression(expr, scope, src, inner_node);
            break;
        }

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
            lower_expression(expr->expr_binary_op.left, scope, src, left_node);

            expr->expr_binary_op.right = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_binary_op.right, scope, src, right_node);
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

        default:
            dbg_node_named(node);
            break;
    }
}

// list helper functions
struct Statement* append_stmt(struct Function* func) {
    func->body_length += 1;
    func->body = realloc(func->body, sizeof(struct Statement) * func->body_length);
    return func->body[func->body_length - 1] = malloc(sizeof(struct Statement));
}

struct Function* append_func(struct Scope* scope) {
    scope->functions_length += 1;
    scope->functions = realloc(scope->functions, sizeof(struct Function) * scope->functions_length);
    return scope->functions[scope->functions_length - 1] = malloc(sizeof(struct Function));
}

struct Variable* append_var(struct Function* func) {
    func->scope.variables_length += 1;
    func->scope.variables = realloc(func->scope.variables, sizeof(struct Variable) * func->scope.variables_length);
    return func->scope.variables[func->scope.variables_length - 1] = malloc(sizeof(struct Variable));
}

struct Variable* append_param(struct Function* func) {
    func->params_length += 1;
    func->params = realloc(func->params, sizeof(struct Variable) * func->params_length);
    return func->params[func->params_length - 1] = malloc(sizeof(struct Variable));
}

void lower_unit(struct Unit* unit, const char* src) {
    unit->scope.functions_length = 0;
    unit->scope.variables_length = 0;

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
                
                struct Function* func = append_func(&unit->scope);
                func->scope.outer = &unit->scope;
                func->scope.functions_length = 0;
                func->scope.variables_length = 0;
                
                func->scope.variables = NULL;
                func->scope.variables_length = 0;
                func->params = NULL;
                func->params_length = 0;
                func->body = NULL;
                func->body_length = 0;

                struct Type* type = malloc(sizeof(struct Type));
                lower_type(src, func_return_type_node, type);
                func->return_type = type;

                TSNode func_identifier_node = ts_node_named_child(func_declarator_node, 0);
                func->identifier = tsnstr(src, func_identifier_node);

                TSNode func_params_node = ts_node_named_child(func_declarator_node, 1);
                size_t func_params_count = ts_node_named_child_count(func_params_node);
                for (int j = 0; i < func_params_count; i++) {
                    TSNode param_node = ts_node_named_child(func_params_node, j);
                    TSNode param_type_node = ts_node_named_child(param_node, 0);
                    TSNode param_ident_node = ts_node_named_child(param_node, 1);
                    struct Type* type = malloc(sizeof(struct Type));
                    lower_type(src, param_type_node, type);
                    
                    struct Variable* param = append_param(func);
                    param->identifier = tsnstr(src, param_ident_node);
                    param->type = type;
                }
                
                size_t func_body_node_children_length = ts_node_named_child_count(func_body_node);
                for (int j = 0; j < func_body_node_children_length; j++) {
                    TSNode stmt_node = ts_node_named_child(func_body_node, j);
                    switch (ts_node_symbol(stmt_node)) {
                        case sym_return_statement: {
                            struct Statement* stmt = append_stmt(func);
                            stmt->kind = STMT_RETURN;

                            struct Expression* expr = &stmt->stmt_return.expr;
                            TSNode expr_node = ts_node_named_child(stmt_node, 0);
                            lower_expression(expr, &func->scope, src, expr_node);
                            break;
                        }

                        case sym_declaration: {
                            TSNode decl_type_node = ts_node_named_child(stmt_node, 0);
                            TSNode decl_ident_node = ts_node_named_child(stmt_node, 1);
                            struct Type* type = malloc(sizeof(struct Type));
                            lower_type(src, decl_type_node, type);
                            
                            struct Variable* local = append_var(func);
                            local->identifier = tsnstr(src, decl_ident_node);
                            local->type = type;
                            break;
                        }

                        case sym_expression_statement: {
                            struct Statement* stmt = append_stmt(func);
                            stmt->kind = STMT_EXPRESSION;

                            struct Expression* expr = &stmt->stmt_expression.expr;
                            TSNode expr_node = ts_node_named_child(stmt_node, 0);
                            lower_expression(expr, &func->scope, src, expr_node);
                            break;
                        }

                        default:
                            dbg_node_named(stmt_node);
                            break;
                    }
                }

                break;
            }

            default:
                dbg_node_named(node);
                break;
        }
    }

#ifdef DBG
    printf("------------------------\n");
#endif

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

#endif