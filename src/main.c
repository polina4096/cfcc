#include <stdio.h>
#include <stdlib.h>

#include "hir.c"
#include "type.c"
#include "codegen.c"
#include "util.c"

int main() {
    // struct Type* type_i32 = malloc(sizeof(struct Type));
    // type_i32->kind  = TYPE_KIND_BASIC;
    // type_i32->basic = TYPE_I32;

    // size_t functions_length = 1;
    // struct Function* functions = malloc(sizeof(struct Function) * functions_length);

    // // int main()
    // functions[0].identifier = "main";
    // functions[0].return_type = type_i32;
    // functions[0].locals_length = 0;
    // functions[0].params_length = 0;

    // functions[0].body_length = 1;
    // functions[0].body = malloc(sizeof(struct Statement) * functions[1].body_length);
    //     functions[0].body->kind = STMT_RETURN;
    //     functions[0].body->stmt_return.expr = malloc(sizeof(struct Expression));
    //         functions[0].body->stmt_return.expr->kind = EXPR_BIN_OP;
    //         functions[0].body->stmt_return.expr->expr_binary_op.kind = BINARY_OP_MUL;
    //         functions[0].body->stmt_return.expr->expr_binary_op.left = malloc(sizeof(struct Expression));
    //             functions[0].body->stmt_return.expr->expr_binary_op.left->kind = EXPR_LITERAL;
    //             functions[0].body->stmt_return.expr->expr_binary_op.left->expr_literal.type = type_i32;
    //             functions[0].body->stmt_return.expr->expr_binary_op.left->expr_literal.value = "10";
    //         functions[0].body->stmt_return.expr->expr_binary_op.right = malloc(sizeof(struct Expression));
    //             functions[0].body->stmt_return.expr->expr_binary_op.right->kind = EXPR_LITERAL;
    //             functions[0].body->stmt_return.expr->expr_binary_op.right->expr_literal.type = type_i32;
    //             functions[0].body->stmt_return.expr->expr_binary_op.right->expr_literal.value = "2";

    // struct Unit unit = {
    //     .functions = functions,
    //     .functions_length = functions_length,

    //     .globals = NULL,
    //     .globals_length = 0,
    // };


    struct Unit unit = {};
	const char* src = read_file("test.c");
    lower_unit(&unit, src);

    // Generation
    struct RegisterAllocator* registers = malloc(sizeof(struct RegisterAllocator));
    
    registers->argument_count = 4;
    registers->argument = malloc(sizeof(const char*) * registers->argument_count);
    registers->argument[0] = "rdi";
    registers->argument[1] = "rsi";
    registers->argument[2] = "rdx";
    registers->argument[3] = "rcx";

    registers->scratch_count = 4;
    registers->scratch_state = calloc(registers->scratch_count, sizeof(int));
    registers->scratch = malloc(sizeof(const char*) * registers->scratch_count);
    registers->scratch[0] = "r8d";
    registers->scratch[1] = "r9d";
    registers->scratch[2] = "r10d";
    registers->scratch[3] = "r11d";

    char* str = generate(&unit, registers);
    printf("%s\n", str);
}