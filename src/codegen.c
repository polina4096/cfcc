#ifndef CFCC_CODEGEN_C
#define CFCC_CODEGEN_C

#include <stdio.h>
#include <stdlib.h>

#include "hir.c"
#include "asm.c"

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

struct Register generate_expr(struct Expression* expr, struct Scope* scope, struct Function* func, struct Context* ctx, char** buffer) {    
    switch (expr->kind) {
        case EXPR_VARIABLE: {
            struct Register r = alloc_register(&ctx->allocator, type_size(expr->expr_variable.variable->type));
            size_t offset = calc_var_offset(&func->scope, expr->expr_variable.variable, NULL);
            load_ir(buffer, offset, r, ctx);
            return r;
        }

        case EXPR_VARIABLE_INDEX: {
            struct Register r = alloc_register(&ctx->allocator, type_size(expr->expr_variable_index.variable->type->array.type));
            struct Register r_index_offset = generate_expr(expr->expr_variable_index.index_expression, scope, func, ctx, buffer);
            size_t stack_offset = calc_var_offset(&func->scope, expr->expr_variable_index.variable, NULL);
            
            load_irr(buffer, stack_offset, r_index_offset, r, ctx);
            free_register(&ctx->allocator, r_index_offset.index);

            return r;
        }

        case EXPR_VARIABLE_POINTER: {
            struct Register r = alloc_register(&ctx->allocator, type_size(expr->expr_variable_index.variable->type->pointer.type));
            size_t offset = calc_var_offset(&func->scope, expr->expr_variable_pointer.variable, NULL);
            read_ir(buffer, offset, r, ctx);
            return r;
        }

        case EXPR_ASSIGNMENT: {
            struct Register r = generate_expr(expr->expr_assignment.expression, scope, func, ctx, buffer);
            size_t offset = calc_var_offset(&func->scope, expr->expr_assignment.variable, NULL);

            // TODO: implement proper type support
            struct Type* type = expr->expr_assignment.variable->type;
            switch (type->kind) {
                case TYPE_KIND_POINTER:
                case TYPE_KIND_BASIC: {
                    switch (type_size(type)) {
                        case 4: {
                            strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp)\n", ctx->allocator.scratch[r.index], offset);
                            break;
                        }

                        case 8: {
                            strfmt(buffer, "\tmovq %%%s, -%i(%%rbp)\n", ctx->allocator.scratch[r.index], offset);
                            break;
                        }

                        default: {
                            printf("assignment to varible of size %i bytes can't be done\n");
                            break;
                        }
                    }
                    break;
                }
                
                case TYPE_KIND_ARRAY: {
                    break;
                }

                case TYPE_KIND_COMPOUND: {
                    printf("compound assignment expressions are not yet supported\n");
                    break;
                }
            }
            
            return r;
        }

        case EXPR_ASSIGNMENT_INDEX: {
            struct Register r = generate_expr(expr->expr_assignment_index.expression, scope, func, ctx, buffer);
            struct Register r_index_offset = generate_expr(expr->expr_assignment_index.index_expression, scope, func, ctx, buffer);
            size_t stack_offset = calc_var_offset(&func->scope, expr->expr_assignment_index.variable, NULL);
            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r_index_offset.index], stack_offset);
            free_register(&ctx->allocator, r_index_offset.index);

            strapp(buffer, "\tcltq\n");
            strfmt(buffer, "\tmovl %%%sd, -%i(%%rbp,%%rax,4)\n", ctx->allocator.scratch[r.index], stack_offset);
            return r;
        }

        case EXPR_ASSIGNMENT_POINTER: {
            struct Register r = generate_expr(expr->expr_assignment_pointer.expression, scope, func, ctx, buffer);
            struct Register r_memory_address = generate_expr(expr->expr_assignment_pointer.expression, scope, func, ctx, buffer);
            size_t offset = calc_var_offset(&func->scope, expr->expr_assignment_pointer.variable, NULL);
            strfmt(buffer, "\tmovq -%i(%%rbp), %%%s\n", offset, ctx->allocator.scratch[r_memory_address.index]);
            strfmt(buffer, "\tmovl %%%sd, (%%%s)\n", ctx->allocator.scratch[r.index], ctx->allocator.scratch[r_memory_address.index]);
            free_register(&ctx->allocator, r_memory_address.index);

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
                            struct Register r = alloc_register(&ctx->allocator, 4);
                            strfmt(buffer, "\tmovl $%s, %%%sd\n", expr->expr_literal.value, ctx->allocator.scratch[r.index]);
                            return r;
                        }

                        case TYPE_I64: {
                            struct Register r = alloc_register(&ctx->allocator, 8);
                            strfmt(buffer, "\tmovq $%s, %%%s\n", expr->expr_literal.value, ctx->allocator.scratch[r.index]);
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

            return (struct Register) { -1, -1 };
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
                struct Register r = generate_expr(expr->expr_call.args[i], scope, func, ctx, buffer);
                arg_r(buffer, i, r, ctx);
                free_register(&ctx->allocator, r.index);
            }

            // call function
            strfmt(buffer, "\tcall %s\n", expr->expr_call.func->identifier);

            // restore scratch registers
            for (int i = 0; i < ctx->allocator.scratch_count; i++) {
                if (ctx->allocator.scratch_state[i] != 0) {
                    strfmt(buffer, "\tpopq %%%s\n", ctx->allocator.scratch[i]);
                }
            }

            struct Type* return_type = expr->expr_call.func->return_type;
            struct Register r = alloc_register(&ctx->allocator, type_size(return_type));
            switch (return_type->kind) {
                case TYPE_KIND_BASIC: {
                    switch (return_type->basic) {
                        case TYPE_VOID:
                            break;

                        case TYPE_I32:
                            strfmt(buffer, "\tmovl %%eax, %%%sd\n", ctx->allocator.scratch[r.index]);
                            break;
                        
                        case TYPE_I64:
                            strfmt(buffer, "\tmovq %%rax, %%%s\n", ctx->allocator.scratch[r.index]);
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
                    struct Register r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
                    struct Register r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r1.index]);
                    strfmt(buffer, "\tjne .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r2.index]);
                    strfmt(buffer, "\tjne .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tmovl $1, %%%sd\n", ctx->allocator.scratch[r1.index]);
                    strfmt(buffer, "\tjmp .L%zu\n", ctx->free_label + 1);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    strfmt(buffer, "\tmovl $0, %%%sd\n", ctx->allocator.scratch[r1.index]);
                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    free_register(&ctx->allocator, r2.index);
                    return r1;
                }

                case BINARY_OP_OR: {
                    struct Register r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
                    struct Register r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r1.index]);
                    strfmt(buffer, "\tje .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r2.index]);
                    strfmt(buffer, "\tje .L%zu\n", ctx->free_label);

                    strfmt(buffer, "\tmovl $0, %%%sd\n", ctx->allocator.scratch[r1.index]);
                    strfmt(buffer, "\tjmp .L%zu\n", ctx->free_label + 1);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    strfmt(buffer, "\tmovl $1, %%%sd\n", ctx->allocator.scratch[r1.index]);

                    strfmt(buffer, ".L%zu:\n", ctx->free_label);
                    ctx->free_label += 1;

                    free_register(&ctx->allocator, r2.index);
                    return r1;
                }
            }

            struct Register r1 = generate_expr(bin_op->left, scope, func, ctx, buffer);
            struct Register r2 = generate_expr(bin_op->right, scope, func, ctx, buffer);
            switch (bin_op->kind) {
                // math
                case BINARY_OP_ADD:
                    add_rr(buffer, r1, r2, ctx);
                    break;

                case BINARY_OP_SUB:
                    sub_rr(buffer, r1, r2, ctx);
                    break;

                case BINARY_OP_MUL:
                    mul_rr(buffer, r1, r2, ctx);
                    break;

                case BINARY_OP_DIV:
                    // TODO: division is hard :\
                    break;

                // relational
                case BINARY_OP_LT:
                    logical_rr(buffer, r1, r2, "l", ctx);
                    break;

                case BINARY_OP_GT:
                    logical_rr(buffer, r1, r2, "g", ctx);
                    break;

                case BINARY_OP_LET:
                    logical_rr(buffer, r1, r2, "le", ctx);
                    break;

                case BINARY_OP_GET:
                    logical_rr(buffer, r1, r2, "ge", ctx);
                    break;

                // equality
                case BINARY_OP_EQ:
                    logical_rr(buffer, r1, r2, "e", ctx);
                    break;

                case BINARY_OP_NE:
                    logical_rr(buffer, r1, r2, "ne", ctx);
                    break;
                }

            free_register(&ctx->allocator, r2.index);
            return r1;
        }
    }

    return (struct Register) { -1, -1 };
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
            struct Register r = generate_expr(&stmt->stmt_if.condition_expr, scope, func, ctx, buffer);
            strfmt(buffer, "\tcmpl $1, %%%sd\n", ctx->allocator.scratch[r.index]);
            free_register(&ctx->allocator, r.index);

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
            struct Register r = generate_expr(&stmt->stmt_return.expr, scope, func, ctx, buffer);
            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r.index]);
            free_register(&ctx->allocator, r.index);

            strfmt(buffer, "\tjmp .%s_exit\n", func->identifier);
            // strfmt(buffer, "\taddq $%zu, %%rsp\n", ctx->frame_size);
            // strapp(buffer, "\tpopq %rbp\n");
            // strapp(buffer, "\tretq\n");
            break;
        }

        case STMT_EXPRESSION: {
            struct Register r = generate_expr(&stmt->stmt_expression.expr, scope, func, ctx, buffer);
            free_register(&ctx->allocator, r.index);
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
        "\t.string\t\"%ld\\n\"\n"
        "\n"
        ".LC2:\n"
        "\t.string\t\"%c\"\n"
        "\n"
        ".LC3:\n"
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
        "printn_long:\n"
        "\tpushq\t%rbp\n"
        "\tmovq\t%rsp, %rbp\n"
        "\tsubq\t$16, %rsp\n"
        "\tmovq\t%rdi, %rsi\n"
        "\tleaq	.LC1(%rip), %rdi\n"
        "\tmovq	$0, %rax\n"
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
        "\tleaq	.LC2(%rip), %rdi\n"
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
        "\tleaq	.LC3(%rip), %rdi\n"
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