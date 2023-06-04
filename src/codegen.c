#ifndef CFCC_CODEGEN_C
#define CFCC_CODEGEN_C

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "hir.c"

static int strapp(char** buffer_ptr, char* appendage) {
    size_t length_appendage = strlen(appendage);

    char* ptr;
    if (*buffer_ptr != NULL) {
        ptr = realloc(*buffer_ptr, strlen(*buffer_ptr) + length_appendage + 1);
        strcat(ptr, appendage);
    } else {
        ptr = malloc(length_appendage + 1);
        memcpy(ptr, appendage, length_appendage);
        ptr[length_appendage] = '\0';
    }

    if (ptr != NULL) {
        *buffer_ptr = ptr;
        return 0; // Success
    } else {
        return -1; // Failed to reallocate
    }
}

static int strfmt(char** buffer_ptr, const char* format, ...) {
    va_list args;
    va_start(args, format);    
    int length = vsnprintf(NULL, 0, format, args);
    char* temp = malloc(length + 1);
    va_end(args);

    va_start(args, format);
    vsnprintf(temp, length + 1, format, args);
    va_end(args);
    
    int result = strapp(buffer_ptr, temp);
    free(temp);

    return result;
}

struct RegisterAllocator {
    int* scratch_state;
    const char** scratch;
    size_t scratch_count;

    const char** argument;
    size_t argument_count;
};

struct Context {
    struct RegisterAllocator allocator;
};

size_t alloc_register(struct RegisterAllocator* registers) {
    for (int i = 0; i < registers->scratch_count; i++) {
        // 0 means register has not been taken yet
        if (registers->scratch_state[i] == 0) {
            // mark the register as taken
            registers->scratch_state[i] = 1;

            // return it's index
            return i;
        }
    }

    return -1;
}

void free_register(struct RegisterAllocator* registers, size_t idx) {
    registers->scratch_state[idx] = 0;
}

void free_all_registers(struct RegisterAllocator* registers) {
    for (int i = 0; i < registers->scratch_count; i++) {
        registers->scratch_state[i] = 0;
    }
}

size_t calc_var_offset(struct Function* func, struct Variable* var) {
    size_t offset = 0;
    for (int i = 0; i < func->params_length; i++) {
        offset += type_size(func->params[i]->type);
        if (var == func->params[i]) {
            return offset;
        }
    }
    
    for (int i = 0; i < func->scope.variables_length; i++) {
        offset += type_size(func->scope.variables[i]->type);
        if (var == func->scope.variables[i]) {
            return offset;
        }
    }

    printf("failed to find var %s", var->identifier);
    return -1;
}

size_t generate_expr(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {    
    switch (expr->kind) {
        case EXPR_VARIABLE: {
            size_t r = alloc_register(&ctx->allocator);
            size_t offset = calc_var_offset(func, expr->expr_variable.variable);
            strfmt(buffer, "\tmovl -%i(%%rbp), %%%sd\n", offset, ctx->allocator.scratch[r]);
            return r;
        }

        case EXPR_ASSIGNMENT: {
            size_t r = generate_expr(expr->expr_assignment.expression, scope, func, ctx, buffer);
            size_t offset = calc_var_offset(func, expr->expr_assignment.variable);
            strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp)\n", ctx->allocator.scratch[r], offset);
            return r;
        }

        case EXPR_LITERAL: {
            struct ExprLiteral* lit = &expr->expr_literal;
            switch (lit->type->kind) {
                case TYPE_KIND_BASIC: {
                    switch (lit->type->basic) {
                        case TYPE_VOID: {
                            break;
                        }

                        case TYPE_I32: {
                            size_t r = alloc_register(&ctx->allocator);
                            strfmt(buffer, "\tmovl $%s, %%%sd\n", expr->expr_literal.value, ctx->allocator.scratch[r]);
                            return r;
                        }

                        // TODO:
                        case TYPE_F32:
                            break;
                        }
                    break;
                }

                // TODO:
                case TYPE_KIND_COMPOUND:
                    break;

                // TODO:
                case TYPE_KIND_ARRAY:
                    break;
            }

            return -1;
        }

        case EXPR_CALL: {
            // store scratch registers
            for (int i = 0; i < ctx->allocator.scratch_count; i++) {
                if (ctx->allocator.scratch_state[i] != 0) {
                    strfmt(buffer, "\tpushq %%%s\n", ctx->allocator.scratch[i]);
                }
            }

            // load args into arg registers
            for (int i = 0; i < expr->expr_call.args_length; i++) {
                size_t r = generate_expr(expr->expr_call.args[i], scope, func, ctx, buffer);
                strfmt(buffer, "\tmovl %%%sd, %%e%s\n", ctx->allocator.scratch[r], ctx->allocator.argument[i]);
                free_register(&ctx->allocator, r);
            }

            // call function
            strfmt(buffer, "\tcall %s\n", expr->expr_call.func->identifier);

            // restore scratch registers
            for (int i = 0; i < ctx->allocator.scratch_count; i++) {
                if (ctx->allocator.scratch_state[i] != 0) {
                    strfmt(buffer, "\tpopq %%%s\n", ctx->allocator.scratch[i]);
                }
            }

            size_t r = alloc_register(&ctx->allocator);
            struct Type* return_type = expr->expr_call.func->return_type;
            switch (return_type->kind) {
                case TYPE_KIND_BASIC: {
                    switch (return_type->basic) {
                        case TYPE_VOID:
                            break;

                        case TYPE_I32:
                            strfmt(buffer, "\tmovl %%eax, %%%sd\n", ctx->allocator.scratch[r]);
                            break;

                        case TYPE_F32:
                            break;
                    }
                    break;
                }

                case TYPE_KIND_COMPOUND: {

                    break;
                }

                case TYPE_KIND_ARRAY: {

                    break;
                }
            }

            return r;
        }

        case EXPR_BIN_OP: {
            struct ExprBinaryOp* bin_op = &expr->expr_binary_op;
            size_t r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
            size_t r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);
            switch (bin_op->kind) {
                case BINARY_OP_ADD:
                    strfmt(buffer, "\taddl %%%sd, %%%sd\n", ctx->allocator.scratch[r2], ctx->allocator.scratch[r1]);
                    break;

                case BINARY_OP_SUB:
                    strfmt(buffer, "\tsubl %%%sd, %%%sd\n", ctx->allocator.scratch[r2], ctx->allocator.scratch[r1]);
                    break;

                case BINARY_OP_DIV:
                    // TODO: division is hard :\
                    break;

                case BINARY_OP_MUL:
                    strfmt(buffer, "\timull %%%sd, %%%sd\n", ctx->allocator.scratch[r2], ctx->allocator.scratch[r1]);
                    break;
            }

            free_register(&ctx->allocator, r2);
            return r1;
        }
    }

    return -1;
}

char* generate(struct Unit* unit, struct Context* ctx) {
    char* buffer = NULL;
    strapp(&buffer,
        "\t.text\n"
        "\t.globl main\n"
        "\t.type  main, @function\n"
        "\n"
        ".LC0:\n"
        "\t.string \"%d\\n\"\n"
    );

    for (int i = 0; i < unit->scope.functions_length; i++) {
        free_all_registers(&ctx->allocator);
        struct Function* func = unit->scope.functions[i];
        strfmt(&buffer, "\n%s:\n", func->identifier);

        // save prevoius base pointer
        strapp(&buffer, "\tpushq %rbp\n");
        
        // load current stack position as base
        strapp(&buffer, "\tmovq %rsp, %rbp\n");

        // store function arguments
        for (int j = 0; j < func->params_length; j++) {
            size_t offset = calc_var_offset(func, func->params[j]);
            strfmt(&buffer, "\tmovl %%e%s, -%i(%%rbp)\n", ctx->allocator.argument[j], offset);
        }

        size_t frame_size = 0; // calculate stack frame size
        for (int j = 0; j < func->params_length; j++) frame_size += type_size(func->params[j]->type);
        for (int j = 0; j < func->scope.variables_length; j++) frame_size += type_size(func->scope.variables[j]->type);

        // align stack frame to 16 bytes
        if (frame_size % 16 != 0) {
            frame_size += 16 - frame_size % 16;
        }

        // allocate stack space for locals and parameters
        strfmt(&buffer, "\tsubq $%zu, %%rsp\n", frame_size);

        // generate statements
        for (int j = 0; j < func->body_length; j++) {
            struct Statement* stmt = func->body[j];
            switch (stmt->kind) {
                case STMT_RETURN: {
                    size_t r = generate_expr(&stmt->stmt_return.expr, &unit->scope, func, ctx, &buffer);
                    strfmt(&buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r]);
                    free_register(&ctx->allocator, r);

                    strfmt(&buffer, "\taddq $%zu, %%rsp\n", frame_size);

                    strapp(&buffer, "\tpopq %rbp\n");
                    strapp(&buffer, "\tretq\n");
                    break;
                }

                case STMT_EXPRESSION: {
                    size_t r = generate_expr(&stmt->stmt_expression.expr, &unit->scope, func, ctx, &buffer);
                    free_register(&ctx->allocator, r);
                    break;
                }
            }
        }

        // free stack space for locals and parameters
        strfmt(&buffer, "\taddq $%zu, %%rsp\n", frame_size);

        strapp(&buffer, "\tpopq %rbp\n");
        strapp(&buffer, "\tretq\n");
    }
    
    return buffer;
}

#endif