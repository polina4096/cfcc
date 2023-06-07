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
    size_t free_label;
    size_t frame_size;
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

size_t calc_var_offset(struct Scope* scope, struct Variable* var, bool* found) {
    size_t offset = 0;
    for (int i = 0; i < scope->variables_length; i++) {
        offset += type_size(scope->variables[i]->type);
        if (var == scope->variables[i]) {
            if (found != NULL) {
                *found = true;
            }

            return offset;
        }
    }

    for (int i = 0; i < scope->statements_length; i++) {
        if (scope->statements[i]->kind == STMT_COMPOUND) {
            size_t scope_offset = calc_var_offset(&scope->statements[i]->stmt_compound.scope, var, found);
            offset += scope_offset;
        }
    }

    return offset;
}

void generate_logical_op(const char* suffix, size_t r1, size_t r2, struct Context* ctx, char** buffer) {
    strfmt(buffer, "\tcmpl %%%sd, %%%sd\n", ctx->allocator.scratch[r2], ctx->allocator.scratch[r1]);
    strfmt(buffer, "\tset%s %%%sb\n", suffix, ctx->allocator.scratch[r1]);
    strfmt(buffer, "\tmovzbl %%%sb, %%%sd\n", ctx->allocator.scratch[r1], ctx->allocator.scratch[r1]);
}

size_t generate_expr(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {    
    switch (expr->kind) {
        case EXPR_VARIABLE: {
            size_t r = alloc_register(&ctx->allocator);
            size_t offset = calc_var_offset(&func->scope, expr->expr_variable.variable, NULL);
            strfmt(buffer, "\tmovl -%i(%%rbp), %%%sd\n", offset, ctx->allocator.scratch[r]);
            return r;
        }

        case EXPR_ASSIGNMENT: {
            size_t r = generate_expr(expr->expr_assignment.expression, scope, func, ctx, buffer);
            size_t offset = calc_var_offset(&func->scope, expr->expr_assignment.variable, NULL);
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
                // math
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

                // relational
                case BINARY_OP_LT:
                    generate_logical_op("l", r1, r2, ctx, buffer);
                    break;

                case BINARY_OP_GT:
                    generate_logical_op("g", r1, r2, ctx, buffer);
                    break;

                case BINARY_OP_LET:
                    generate_logical_op("le", r1, r2, ctx, buffer);
                    break;

                case BINARY_OP_GET:
                    generate_logical_op("ge", r1, r2, ctx, buffer);
                    break;

                // equality
                case BINARY_OP_EQ:
                    generate_logical_op("e", r1, r2, ctx, buffer);
                    break;

                case BINARY_OP_NE:
                    generate_logical_op("ne", r1, r2, ctx, buffer);
                    break;
                }

            free_register(&ctx->allocator, r2);
            return r1;
        }
    }

    return -1;
}

void generate_statement(struct Statement* stmt, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer);

void generate_scope(struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {
    for (int i = 0; i < scope->statements_length; i++) {
        struct Statement* stmt = scope->statements[i];
        generate_statement(stmt, scope, func, ctx, buffer);
    }
}

void generate_statement(struct Statement* stmt, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {
    switch (stmt->kind) {
        case STMT_COMPOUND: {
            struct Scope* compound_scope = &stmt->stmt_compound.scope;
            generate_scope(compound_scope, func, ctx, buffer);
            break;
        }

        case STMT_GOTO: {
            strfmt(buffer, "\tjmp %s\n", stmt->stmt_goto.label);
            break;
        }

        case STMT_LABEL: {
            strfmt(buffer, "%s:\n", stmt->stmt_label.label);
            generate_scope(&stmt->stmt_label.scope, func, ctx, buffer);
            break;
        }
        
        case STMT_IF:
        case STMT_IF_ELSE: {
            size_t r = generate_expr(&stmt->stmt_if.condition_expr, scope, func, ctx, buffer);
            strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r]);
            free_register(&ctx->allocator, r);
            
            strfmt(buffer, "\tjne .L%zu\n", ctx->free_label);
            generate_scope(&stmt->stmt_if.success_scope, func, ctx, buffer);
            strfmt(buffer, ".L%zu:\n", ctx->free_label);
            ctx->free_label += 1;
            
            if (stmt->kind == STMT_IF_ELSE) {
                generate_scope(&stmt->stmt_if.failure_scope, func, ctx, buffer);
            }

            break;
        }

        case STMT_RETURN: {
            size_t r = generate_expr(&stmt->stmt_return.expr, scope, func, ctx, buffer);
            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r]);
            free_register(&ctx->allocator, r);

            strfmt(buffer, "\tjmp .%s_exit\n", func->identifier);
            // strfmt(buffer, "\taddq $%zu, %%rsp\n", ctx->frame_size);
            // strapp(buffer, "\tpopq %rbp\n");
            // strapp(buffer, "\tretq\n");
            break;
        }

        case STMT_EXPRESSION: {
            size_t r = generate_expr(&stmt->stmt_expression.expr, scope, func, ctx, buffer);
            free_register(&ctx->allocator, r);
            break;
        }
    }
}

size_t calc_scope_frame_size(struct Scope* scope) {
    size_t frame_size = 0;
    for (int j = 0; j < scope->variables_length; j++) frame_size += type_size(scope->variables[j]->type);
    for (int j = 0; j < scope->statements_length; j++) {
        switch (scope->statements[j]->kind) {
            case STMT_COMPOUND:
                frame_size += calc_scope_frame_size(&scope->statements[j]->stmt_compound.scope);
                break;

            case STMT_IF:
                frame_size += calc_scope_frame_size(&scope->statements[j]->stmt_if.success_scope);
                break;

            default: break;
        }
    }

    return frame_size;
}

char* generate(struct Unit* unit, struct Context* ctx) {
    char* buffer = NULL;
    strapp(&buffer,
        "\t.text\n"
        "\t.globl main\n"
        "\t.type  main, @function\n"
    );

    ctx->free_label = 0;

    for (int i = 0; i < unit->scope.functions_length; i++) {
        free_all_registers(&ctx->allocator);
        struct Function* func = unit->scope.functions[i];
        strfmt(&buffer, "\n%s:\n", func->identifier);

        // save prevoius base pointer
        strapp(&buffer, "\tpushq %rbp\n");
        
        // load current stack position as base
        strapp(&buffer, "\tmovq %rsp, %rbp\n");

        // calculate stack frame size
        ctx->frame_size = calc_scope_frame_size(&func->scope);
        for (int j = 0; j < func->params_length; j++) ctx->frame_size += type_size(func->params[j]->type);

        // align stack frame to 16 bytes
        if (ctx->frame_size % 16 != 0) {
            ctx->frame_size += 16 - ctx->frame_size % 16;
        }

        // allocate stack space for locals and parameters
        strfmt(&buffer, "\tsubq $%zu, %%rsp\n", ctx->frame_size);

        // store function arguments
        for (int j = 0; j < func->params_length; j++) {
            size_t offset = calc_var_offset(&func->scope, func->params[j], NULL);
            strfmt(&buffer, "\tmovl %%e%s, -%i(%%rbp)\n", ctx->allocator.argument[j], offset);
        }

        // generate statements
        for (int j = 0; j < func->scope.statements_length; j++) {
            struct Statement* stmt = func->scope.statements[j];
            generate_statement(stmt, &func->scope, func, ctx, &buffer);
        }

        // add exit label (avoids code duplication, adds one jump)
        strfmt(&buffer, ".%s_exit:\n", func->identifier);

        // free stack space for locals and parameters
        strfmt(&buffer, "\taddq $%zu, %%rsp\n", ctx->frame_size);

        strapp(&buffer, "\tpopq %rbp\n");
        strapp(&buffer, "\tretq\n");
    }
    
    return buffer;
}

#endif