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

    struct Statement** statements;
    size_t statements_length;
};


struct Function {
    char* identifier;
    
    struct Variable** params;
    size_t params_length;

    struct Type* return_type;

    struct Scope scope;
    bool prototype;
};

// Compilation Unit
struct Unit {
    struct Scope scope;
};

// Expressions
struct ExprVariable {
    struct Variable* variable;
};

struct ExprVariableIndex {
    struct Variable* variable;
    struct Expression* index_expression;
};

struct ExprAssignment {
    struct Variable* variable;
    struct Expression* expression;
};

struct ExprAssignmentIndex {
    struct Variable* variable;
    struct Expression* expression;
    struct Expression* index_expression;
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
    // math
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_DIV,
    BINARY_OP_MUL,
    
    // relational
    BINARY_OP_LT,
    BINARY_OP_GT,
    BINARY_OP_LET,
    BINARY_OP_GET,

    // equality
    BINARY_OP_EQ,
    BINARY_OP_NE,

    // logical
    BINARY_OP_AND,
    BINARY_OP_OR,
};

struct ExprBinaryOp {
    enum BinaryOperation kind;
    struct Expression* left;
    struct Expression* right;
};

enum ExpressionKind {
    EXPR_VARIABLE,
    EXPR_VARIABLE_INDEX,
    EXPR_ASSIGNMENT,
    EXPR_ASSIGNMENT_INDEX,
    EXPR_LITERAL,
    EXPR_CALL,
    EXPR_BIN_OP,
};

struct Expression {
    enum ExpressionKind kind;
    union {
        struct ExprVariable expr_variable;
        struct ExprVariableIndex expr_variable_index;
        struct ExprAssignment expr_assignment;
        struct ExprAssignmentIndex expr_assignment_index;
        struct ExprLiteral expr_literal;
        struct ExprCall expr_call;
        struct ExprBinaryOp expr_binary_op;
    };
};

// Statements
struct Statement;

struct StmtCompound {
    struct Scope scope;
};

struct StmtGoto {
    const char* label;
};

struct StmtLabel {
    const char* label;
    struct Scope scope;
};

struct StmtIf {
    struct Expression condition_expr;
    struct Scope success_scope;
    struct Scope failure_scope;
};

struct StmtReturn {
    struct Expression expr;
};

struct StmtExpression {
    struct Expression expr;
};

enum StatementKind {
    STMT_COMPOUND,
    STMT_GOTO,
    STMT_LABEL,
    STMT_IF,
    STMT_IF_ELSE,
    STMT_RETURN,
    STMT_EXPRESSION,
};

struct Statement {
    enum StatementKind kind;
    union {
        struct StmtCompound stmt_compound;
        struct StmtGoto stmt_goto;
        struct StmtLabel stmt_label;
        struct StmtIf stmt_if;
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

    printf("\n%i %s\n", ts_node_symbol(node), ts_node_type(node));
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

    else if (strcmp(str, "<") == 0)
        return BINARY_OP_LT;

    else if (strcmp(str, ">") == 0)
        return BINARY_OP_GT;

    else if (strcmp(str, "<=") == 0)
        return BINARY_OP_LET;

    else if (strcmp(str, ">=") == 0)
        return BINARY_OP_GET;

    else if (strcmp(str, "==") == 0)
        return BINARY_OP_EQ;

    else if (strcmp(str, "!=") == 0)
        return BINARY_OP_NE;

    else if (strcmp(str, "&&") == 0)
        return BINARY_OP_AND;

    else if (strcmp(str, "||") == 0)
        return BINARY_OP_OR;

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

// init helper functions
void init_scope(struct Scope* scope, struct Scope* outer) {
    scope->outer = outer;

    scope->functions_length = 0;
    scope->variables_length = 0;
    scope->statements_length = 0;

    scope->functions = NULL;
    scope->variables = NULL;
    scope->statements = NULL;

    scope->variables = NULL;
    scope->variables_length = 0;
}

void init_func(struct Function* func, struct Scope* outer) {
    func->params = NULL;
    func->params_length = 0;

    func->prototype = false;

    init_scope(&func->scope, outer);
}

// list helper functions
struct Statement* append_stmt(struct Scope* scope) {
    scope->statements_length += 1;
    scope->statements = realloc(scope->statements, sizeof(struct Statement) * scope->statements_length);
    return scope->statements[scope->statements_length - 1] = malloc(sizeof(struct Statement));
}

struct Expression* append_arg(struct ExprCall* call_expr) {
    call_expr->args_length += 1;
    call_expr->args = realloc(call_expr->args, sizeof(struct Statement) * call_expr->args_length);
    return call_expr->args[call_expr->args_length - 1] = malloc(sizeof(struct Expression));
}

struct Function* append_func(struct Scope* scope) {
    scope->functions_length += 1;
    scope->functions = realloc(scope->functions, sizeof(struct Function) * scope->functions_length);
    struct Function* func = malloc(sizeof(struct Function));
    init_func(func, scope);
    return scope->functions[scope->functions_length - 1] = func;
}

struct Variable* append_var(struct Scope* scope) {
    scope->variables_length += 1;
    scope->variables = realloc(scope->variables, sizeof(struct Variable) * scope->variables_length);
    return scope->variables[scope->variables_length - 1] = malloc(sizeof(struct Variable));
}

struct Variable* append_param(struct Function* func) {
    func->params_length += 1;
    func->scope.variables_length += 1;
    func->params = realloc(func->params, sizeof(struct Variable) * func->params_length);
    func->scope.variables = realloc(func->scope.variables, sizeof(struct Variable) * func->scope.variables_length);
    return func->params[func->params_length - 1] = func->scope.variables[func->scope.variables_length - 1] = malloc(sizeof(struct Variable));
}

// ast -> hir
void lower_expression(struct Expression* expr, struct Scope* scope, const char* src, TSNode node) {
    switch (ts_node_symbol(node)) {
        case sym_call_expression: {
            expr->kind = EXPR_CALL;
            expr->expr_call.args = NULL;
            expr->expr_call.args_length = 0;
            
            TSNode ident_node = ts_node_named_child(node, 0);
            char* identifier = tsnstr(src, ident_node);
            expr->expr_call.func = find_func(identifier, scope);
            
            if (expr->expr_call.func == NULL) {
                printf("function `%s` not found\n", identifier);
            }

            TSNode args_node = ts_node_named_child(node, 1);
            size_t args_count = ts_node_named_child_count(args_node);
            for (int j = 0; j < args_count; j++) {
                TSNode arg_node = ts_node_named_child(args_node, j);
                struct Expression* arg = append_arg(&expr->expr_call);
                lower_expression(arg, scope, src, arg_node);
            }

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

        case sym_subscript_expression: {
            expr->kind = EXPR_VARIABLE_INDEX;

            TSNode ident_node = ts_node_named_child(node, 0);
            TSNode index_expr_node = ts_node_named_child(node, 1);
            char* identifier = tsnstr(src, ident_node);

            expr->expr_variable_index.variable = find_var(identifier, scope);
            expr->expr_variable_index.index_expression = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_variable_index.index_expression, scope, src, index_expr_node);

            if (expr->expr_variable_index.variable == NULL) {
                printf("variable `%s` not found\n", identifier);
            }


            break;
        }

        case sym_assignment_expression: {
            TSNode var_node = ts_node_named_child(node, 0);
            int var_node_sym = ts_node_symbol(var_node);

            switch (var_node_sym) {
                case sym_identifier: {
                    expr->kind = EXPR_ASSIGNMENT;
                    char* identifier = tsnstr(src, var_node);
                    expr->expr_assignment.variable = find_var(identifier, scope);

                    if (expr->expr_assignment.variable == NULL) {
                        printf("variable `%s` not found\n", identifier);
                    }

                    break;
                }

                case sym_subscript_expression: {
                    expr->kind = EXPR_ASSIGNMENT_INDEX;
                    TSNode ident_node = ts_node_named_child(var_node, 0);
                    TSNode index_expr_node = ts_node_named_child(var_node, 1);
                    char* identifier = tsnstr(src, ident_node);

                    expr->expr_assignment_index.variable = find_var(identifier, scope);
                    expr->expr_assignment_index.index_expression = malloc(sizeof(struct Expression));
                    lower_expression(expr->expr_assignment_index.index_expression, scope, src, index_expr_node);

                    if (expr->expr_assignment.variable == NULL) {
                        printf("variable `%s` not found\n", identifier);
                    }

                    break;
                }

            }

            TSNode expr_node = ts_node_named_child(node, 1);
            expr->expr_assignment.expression = malloc(sizeof(struct Expression));
            lower_expression(expr->expr_assignment.expression, scope, src, expr_node);
            
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

void lower_statement(struct Scope* scope, const char* src, TSNode node) {
    switch (ts_node_symbol(node)) {
        case sym_compound_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_COMPOUND;

            struct Scope* compound_scope = &stmt->stmt_compound.scope;
            init_scope(compound_scope, scope);

            size_t cmpd_stmt_node_children_length = ts_node_named_child_count(node);
            for (int i = 0; i < cmpd_stmt_node_children_length; i++) {
                TSNode stmt_node = ts_node_named_child(node, i);
                lower_statement(compound_scope, src, stmt_node);
            }

            break;
        }

        case sym_labeled_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_LABEL;
            stmt->stmt_label.label = tsnstr(src, ts_node_named_child(node, 0));
            init_scope(&stmt->stmt_label.scope, scope);
            lower_statement(&stmt->stmt_label.scope, src, ts_node_named_child(node, 1));
            break;
        }

        case sym_goto_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_GOTO;
            stmt->stmt_goto.label = tsnstr(src, ts_node_named_child(node, 0));
            break;
        }

        case sym_if_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_IF;
            
            struct Scope* success_scope = &stmt->stmt_if.success_scope;
            struct Scope* failure_scope = &stmt->stmt_if.failure_scope;
            init_scope(success_scope, scope);
            init_scope(failure_scope, scope);

            TSNode condition_expr_node = ts_node_named_child(node, 0);
            struct Expression* condition_expr = &stmt->stmt_if.condition_expr;
            lower_expression(condition_expr, scope, src, condition_expr_node);

            TSNode success_compound_node = ts_node_named_child(node, 1);
            lower_statement(success_scope, src, success_compound_node);

            // else branch
            if (ts_node_named_child_count(node) > 2) {
                stmt->kind = STMT_IF_ELSE;
                TSNode failure_compound_node = ts_node_named_child(node, 2);
                lower_statement(failure_scope, src, failure_compound_node);
            }
            break;
        }

        case sym_return_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_RETURN;

            struct Expression* expr = &stmt->stmt_return.expr;
            TSNode expr_node = ts_node_named_child(node, 0);
            lower_expression(expr, scope, src, expr_node);
            break;
        }

        case sym_declaration: {
            TSNode decl_type_node = ts_node_named_child(node, 0);
            TSNode decl_decl_node = ts_node_named_child(node, 1);
            struct Type* type = malloc(sizeof(struct Type));
            lower_type(src, decl_type_node, type);
            
            struct Variable* local = append_var(scope);
            local->type = type;

            // declaration declarator = decl decl :')
            int decl_decl_node_sym = ts_node_symbol(decl_decl_node);
            switch (decl_decl_node_sym) {
                case sym_identifier: {
                    // <identifier>
                    local->identifier = tsnstr(src, decl_decl_node);

                    break;
                }

                case sym_init_declarator: {
                    // <identifier> = <expression>
                    TSNode ident_node = ts_node_named_child(decl_decl_node, 0);
                    TSNode expr_node = ts_node_named_child(decl_decl_node, 1);

                    local->identifier = tsnstr(src, ident_node);

                    struct Statement* stmt = append_stmt(scope);
                    stmt->kind = STMT_EXPRESSION;

                    struct Expression* expr = &stmt->stmt_expression.expr;
                    expr->kind = EXPR_ASSIGNMENT;
                    expr->expr_assignment.variable = local;
                    expr->expr_assignment.expression = malloc(sizeof(struct Expression));
                    lower_expression(expr->expr_assignment.expression, scope, src, expr_node);

                    break;
                }

                case sym_array_declarator: {
                    // <identifier>[<number_literal>]
                    TSNode ident_node = ts_node_named_child(decl_decl_node, 0);
                    TSNode length_node = ts_node_named_child(decl_decl_node, 1);
                    char* length_str = tsnstr(src, length_node);

                    local->identifier = tsnstr(src, ident_node);
                    local->type = malloc(sizeof(struct Type));
                    local->type->kind = TYPE_KIND_ARRAY;
                    local->type->array.type = type;

                    size_t length = strtol(length_str, NULL, 10);
                    if (length == 0) {
                        printf("failed to parse array length, must be const\n");
                    }

                    local->type->array.length = length;
                    free(length_str);

                    break;
                }
            }

            break;
        }

        case sym_expression_statement: {
            struct Statement* stmt = append_stmt(scope);
            stmt->kind = STMT_EXPRESSION;

            struct Expression* expr = &stmt->stmt_expression.expr;
            TSNode expr_node = ts_node_named_child(node, 0);
            lower_expression(expr, scope, src, expr_node);
            break;
        }

        default:
            dbg_node_named(node);
            break;
    }
}

void lower_unit(struct Unit* unit, const char* src) {
    init_scope(&unit->scope, NULL);

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
            case sym_declaration: {
                TSNode func_return_type_node = ts_node_named_child(node, 0);
                TSNode func_declarator_node = ts_node_named_child(node, 1);

                struct Function* func = append_func(&unit->scope);
                struct Type* type = malloc(sizeof(struct Type));
                lower_type(src, func_return_type_node, type);
                func->return_type = type;

                TSNode func_identifier_node = ts_node_named_child(func_declarator_node, 0);
                func->identifier = tsnstr(src, func_identifier_node);

                TSNode func_params_node = ts_node_named_child(func_declarator_node, 1);
                size_t func_params_count = ts_node_named_child_count(func_params_node);
                for (int j = 0; j < func_params_count; j++) {
                    TSNode param_node = ts_node_named_child(func_params_node, j);
                    TSNode param_type_node = ts_node_named_child(param_node, 0);
                    TSNode param_ident_node = ts_node_named_child(param_node, 1);
                    struct Type* type = malloc(sizeof(struct Type));
                    lower_type(src, param_type_node, type);

                    struct Variable* param = append_param(func);
                    param->identifier = tsnstr(src, param_ident_node);
                    param->type = type;
                }

                func->prototype = true;
                break;
            }

            case sym_function_definition: {
                TSNode func_return_type_node = ts_node_named_child(node, 0);
                TSNode func_declarator_node = ts_node_named_child(node, 1);
                TSNode func_cmpd_stmt_node = ts_node_named_child(node, 2);
                
                struct Function* func = append_func(&unit->scope);
                struct Type* type = malloc(sizeof(struct Type));
                lower_type(src, func_return_type_node, type);
                func->return_type = type;

                TSNode func_identifier_node = ts_node_named_child(func_declarator_node, 0);
                func->identifier = tsnstr(src, func_identifier_node);

                TSNode func_params_node = ts_node_named_child(func_declarator_node, 1);
                size_t func_params_count = ts_node_named_child_count(func_params_node);
                for (int j = 0; j < func_params_count; j++) {
                    TSNode param_node = ts_node_named_child(func_params_node, j);
                    TSNode param_type_node = ts_node_named_child(param_node, 0);
                    TSNode param_ident_node = ts_node_named_child(param_node, 1);
                    struct Type* type = malloc(sizeof(struct Type));
                    lower_type(src, param_type_node, type);
                    
                    struct Variable* param = append_param(func);
                    param->identifier = tsnstr(src, param_ident_node);
                    param->type = type;
                }
                
                size_t func_cmpd_stmt_node_children_length = ts_node_named_child_count(func_cmpd_stmt_node);
                for (int j = 0; j < func_cmpd_stmt_node_children_length; j++) {
                    TSNode stmt_node = ts_node_named_child(func_cmpd_stmt_node, j);
                    lower_statement(&func->scope, src, stmt_node);
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