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

struct LValue {
    size_t offset;
    size_t r_index;
    size_t r_address;
    // in case lvalue is actually an expression
    size_t r_expr;
};

size_t generate_expr(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer);

struct LValue generate_lvalue(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {
    switch (expr->kind) {
        case EXPR_VARIABLE: {
            size_t offset = calc_var_offset(&func->scope, expr->expr_variable.variable, NULL);

            struct LValue out;
            out.offset = offset;
            out.r_index = -1;
            out.r_address = -1;
            out.r_expr = -1;
            return out;
        }

        case EXPR_INDEX: {
            struct LValue lval_location = generate_lvalue(expr->expr_index.location, scope, func, ctx, buffer);
            struct LValue lval_index;

            if (lval_location.r_expr != -1) {
                lval_index = lval_location;
                lval_location = generate_lvalue(expr->expr_index.expression, scope, func, ctx, buffer);
            } else {
                lval_index.offset = -1;
                lval_index.r_index = -1;
                lval_index.r_address = -1;
                lval_index.r_expr = generate_expr(expr->expr_index.expression, scope, func, ctx, buffer);
            }

            lval_location.r_index = lval_index.r_expr;
            return lval_location;
        }
    }

    struct LValue out;
    out.offset = -1;
    out.r_index = -1;
    out.r_address = -1;
    out.r_expr = generate_expr(expr, scope, func, ctx, buffer);
    return out;
}

size_t generate_expr(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {    
    switch (expr->kind) {
        case EXPR_VARIABLE: {
            size_t r = alloc_register(&ctx->allocator);
            size_t offset = calc_var_offset(&func->scope, expr->expr_variable.variable, NULL);
            strfmt(buffer, "\tmovl -%i(%%rbp), %%%sd\n", offset, ctx->allocator.scratch[r]);
            return r;
        }

        case EXPR_INDEX: {
            size_t r = alloc_register(&ctx->allocator);
            struct LValue lval_location = generate_lvalue(expr->expr_index.location, scope, func, ctx, buffer);
            struct LValue lval_index;

            if (lval_location.r_expr != -1) {
                lval_index = lval_location;
                lval_location = generate_lvalue(expr->expr_index.expression, scope, func, ctx, buffer);
            } else {
                lval_index.offset = -1;
                lval_index.r_index = -1;
                lval_index.r_address = -1;
                lval_index.r_expr = generate_expr(expr->expr_index.expression, scope, func, ctx, buffer);
            }

            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[lval_index.r_expr]);
            free_register(&ctx->allocator, lval_index.r_expr);

            strapp(buffer, "\tcltq\n");
            if (lval_location.r_address == -1) {
                strfmt(buffer, "\tmovl -%i(%%rbp,%%rax,4), %%%sd\n", lval_location.offset, ctx->allocator.scratch[r]);
            } else {
                strfmt(buffer, "\tmovl (%%%s,%%%s,4), %%%sd\n", ctx->allocator.scratch[lval_location.r_address], ctx->allocator.scratch[lval_location.r_index], ctx->allocator.scratch[r]);
                free_register(&ctx->allocator, lval_index.r_index);
                free_register(&ctx->allocator, lval_index.r_address);
            }

            return r;
        }

        case EXPR_ADDRESS_OF: {
            struct LValue lval = generate_lvalue(expr->expr_address_of.expression, scope, func, ctx, buffer);
            if (lval.r_address == -1)  {
                size_t r = alloc_register(&ctx->allocator);
                strfmt(buffer, "\tleaq -%i(%%rbp), %%%s\n", lval.offset, ctx->allocator.scratch[r]);
                return r;
            } else {
                return lval.r_address;
            }
        }

        case EXPR_ASSIGNMENT: {
            struct LValue lval = generate_lvalue(expr->expr_assignment.location, scope, func, ctx, buffer);
            size_t r = generate_expr(expr->expr_assignment.expression, scope, func, ctx, buffer);
            
            if (lval.r_index != -1) {
                // stack array
                strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[lval.r_index]);
                free_register(&ctx->allocator, lval.r_index);
                
                strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp,%%rax,4)\n", ctx->allocator.scratch[r], lval.offset);
            } else if (lval.r_address == -1)  {
                // stack
                strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp)\n", ctx->allocator.scratch[r], lval.offset);
            } else {
                // heap/stack
                strfmt(buffer, "\tmovl %%%sd, (%%%s)\n", ctx->allocator.scratch[r], ctx->allocator.scratch[lval.r_address]);
            }

            return r;
        }

        // case EXPR_ASSIGNMENT_INDEX: {
        //     size_t r = generate_expr(expr->expr_assignment_index.expression, scope, func, ctx, buffer);
        //     size_t r_index_offset = generate_expr(expr->expr_assignment_index.index_expression, scope, func, ctx, buffer);
        //     size_t stack_offset = calc_var_offset(&func->scope, expr->expr_assignment_index.variable, NULL);
        //     strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r_index_offset], stack_offset);
        //     free_register(&ctx->allocator, r_index_offset);

        //     strapp(buffer, "\tcltq\n");
        //     strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp,%%rax,4)\n", ctx->allocator.scratch[r], stack_offset);
        //     return r;
        // }

        // case EXPR_ASSIGNMENT_POINTER: {
        //     size_t r = generate_expr(expr->expr_assignment_pointer.expression, scope, func, ctx, buffer);
        //     size_t r_memory_address = generate_expr(expr->expr_assignment_pointer.expression, scope, func, ctx, buffer);
        //     size_t offset = calc_var_offset(&func->scope, expr->expr_assignment_pointer.variable, NULL);
        //     strfmt(buffer, "\tmovq -%i(%%rbp), %%%s\n", offset, ctx->allocator.scratch[r_memory_address]);
        //     strfmt(buffer, "\tmovl %%%sd, (%%%s)\n", ctx->allocator.scratch[r], ctx->allocator.scratch[r_memory_address]);
        //     free_register(&ctx->allocator, r_memory_address);

        //     return r;
        // }

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
            switch (bin_op->kind) {
                // logical
                case BINARY_OP_AND: {
                    size_t r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
                    size_t r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r1]);
                    strfmt(buffer, "\tjne .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r2]);
                    strfmt(buffer, "\tjne .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tmovl $1, %%%sd\n", ctx->allocator.scratch[r1]);
                    strfmt(buffer, "\tjmp .L%zu\n", ctx->free_label + 1);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    strfmt(buffer, "\tmovl $0, %%%sd\n", ctx->allocator.scratch[r1]);
                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    free_register(&ctx->allocator, r2);
                    return r1;
                }

                case BINARY_OP_OR: {
                    size_t r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
                    size_t r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r1]);
                    strfmt(buffer, "\tje .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r2]);
                    strfmt(buffer, "\tje .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tmovl $0, %%%sd\n", ctx->allocator.scratch[r1]);
                    strfmt(buffer, "\tjmp .L%zu\n", ctx->free_label + 1);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    strfmt(buffer, "\tmovl $1, %%%sd\n", ctx->allocator.scratch[r1]);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    free_register(&ctx->allocator, r2);
                    return r1;
                }
            }

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

            if (stmt->kind == STMT_IF_ELSE) {
                size_t label_else = ctx->free_label;
                ctx->free_label += 1;

                size_t label_end = ctx->free_label;
                ctx->free_label += 1;

                strfmt(buffer, "\tjne .L%zu\n", label_else);

                generate_scope(&stmt->stmt_if.success_scope, func, ctx, buffer);
                strfmt(buffer, "\tjmp .L%zu\n", label_end);

                strfmt(buffer, ".L%zu:\n", label_else);
                generate_scope(&stmt->stmt_if.failure_scope, func, ctx, buffer);

                strfmt(buffer, ".L%zu:\n", label_end);
            } else {
                size_t label_end = ctx->free_label;
                ctx->free_label += 1;

                strfmt(buffer, "\tjne .L%zu\n", label_end);
                generate_scope(&stmt->stmt_if.success_scope, func, ctx, buffer);

                strfmt(buffer, ".L%zu:\n", label_end);
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
        "\n"
        ".LC0:\n"
        "\t.string\t\"%d\\n\"\n"
        "\n"
        ".LC1:\n"
        "\t.string\t\"%c\"\n"
        "\n"
        ".LC2:\n"
        "\t.string\t\"\\n\"\n"
        "\n"
        "printn_int:\n"
        "\tpushq\t%rbp\n"
        "\tmovq\t%rsp, %rbp\n"
        "\tsubq\t$16, %rsp\n"
        "\tmovl\t%edi, -4(%rbp)\n"
        "\tmovl\t-4(%rbp), %eax\n"
        "\tmovl\t%eax, %esi\n"
        "\tleaq	.LC0(%rip), %rdi\n"
        "\tmovl	$0, %eax\n"
        "\tcall	printf@PLT\n"
        "\tnop\n"
        "\tleave\n"
        "\tret\n"
        "\n"
        "print_char:\n"
        "\tpushq\t%rbp\n"
        "\tmovq\t%rsp, %rbp\n"
        "\tsubq\t$16, %rsp\n"
        "\tmovl\t%edi, -4(%rbp)\n"
        "\tmovl\t-4(%rbp), %eax\n"
        "\tmovl\t%eax, %esi\n"
        "\tleaq	.LC1(%rip), %rdi\n"
        "\tmovl	$0, %eax\n"
        "\tcall	printf@PLT\n"
        "\tnop\n"
        "\tleave\n"
        "\tret\n"
        "\n"
        "print_newline:\n"
        "\tpushq\t%rbp\n"
        "\tmovq\t%rsp, %rbp\n"
        "\tsubq\t$16, %rsp\n"
        "\tmovl\t-4(%rbp), %eax\n"
        "\tmovl\t%eax, %esi\n"
        "\tleaq	.LC2(%rip), %rdi\n"
        "\tmovl	$0, %eax\n"
        "\tcall	printf@PLT\n"
        "\tnop\n"
        "\tleave\n"
        "\tret\n"
    );

    ctx->free_label = 0;

    for (int i = 0; i < unit->scope.functions_length; i++) {
        struct Function* func = unit->scope.functions[i];
        if (func->prototype) continue;

        free_all_registers(&ctx->allocator);
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