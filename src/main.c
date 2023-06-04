#include <stdio.h>
#include <stdlib.h>

#include "hir.c"
#include "type.c"
#include "codegen.c"
#include "util.c"

int main() {
    struct Unit unit = {};
	const char* src = read_file("test.c");
    lower_unit(&unit, src);

    // Generation
    struct Context* ctx = malloc(sizeof(struct Context));
    
    ctx->allocator.argument_count = 4;
    ctx->allocator.argument = malloc(sizeof(const char*) * ctx->allocator.argument_count);
    ctx->allocator.argument[0] = "di";
    ctx->allocator.argument[1] = "si";
    ctx->allocator.argument[2] = "dx";
    ctx->allocator.argument[3] = "cx";

    ctx->allocator.scratch_count = 4;
    ctx->allocator.scratch_state = calloc(ctx->allocator.scratch_count, sizeof(int));
    ctx->allocator.scratch = malloc(sizeof(const char*) * ctx->allocator.scratch_count);
    ctx->allocator.scratch[0] = "r8";
    ctx->allocator.scratch[1] = "r9";
    ctx->allocator.scratch[2] = "r10";
    ctx->allocator.scratch[3] = "r11";

    char* str = generate(&unit, ctx);
    printf("%s\n", str);
}