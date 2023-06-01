#ifndef CFCC_HIR_C
#define CFCC_HIR_C

#include <stddef.h>

#include "type.c"

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

struct Unit {
    struct Function* functions;
    size_t functions_length;
    
    struct Variable* globals;
    size_t globals_length;
};

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

#endif