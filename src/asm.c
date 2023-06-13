#ifndef CFCC_ASM_C
#define CFCC_ASM_C

#include "util.c"

struct RegisterAllocator {
    int* scratch_state;
    const char** scratch;
    size_t scratch_count;

    const char** argument;
    size_t argument_count;
};

struct Register {
    size_t index;
    size_t size;
};

struct Context {
    struct RegisterAllocator allocator;
    size_t free_label;
    size_t frame_size;
};

struct Register alloc_register(struct RegisterAllocator* registers, size_t size) {
    for (int i = 0; i < registers->scratch_count; i++) {
        // 0 means register has not been taken yet
        if (registers->scratch_state[i] == 0) {
            // mark the register as taken
            registers->scratch_state[i] = 1;

            // return it's index
            return (struct Register) { .index = i, .size = size };
        }
    }

    return (struct Register) { -1, -1 };
}

void free_register(struct RegisterAllocator* registers, size_t idx) {
    registers->scratch_state[idx] = 0;
}

void free_all_registers(struct RegisterAllocator* registers) {
    for (int i = 0; i < registers->scratch_count; i++) {
        registers->scratch_state[i] = 0;
    }
}

const char* temp_r(char** buffer, struct Register r, struct Context* ctx) {
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tmovq %%%s, %%rax\n", ctx->allocator.scratch[r.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }

    return "ax";
}

const char* arg_r(char** buffer, size_t arg_idx, struct Register r, struct Context* ctx) {
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tmovl %%%sd, %%e%s\n", ctx->allocator.scratch[r.index], ctx->allocator.argument[arg_idx]);
            break;
        
        case 8:
            strfmt(buffer, "\tmovq %%%s, %%r%s\n", ctx->allocator.scratch[r.index], ctx->allocator.argument[arg_idx]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }

    return "ax";
}

void load_ir(char** buffer, size_t offset, struct Register r, struct Context* ctx) {
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tmovl -%i(%%rbp), %%%sd\n", offset, ctx->allocator.scratch[r.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tmovq -%i(%%rbp), %%%s\n", offset, ctx->allocator.scratch[r.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void load_irr(char** buffer, size_t offset, struct Register r_offset, struct Register r, struct Context* ctx) {
    const char* r_temp = temp_r(buffer, r, ctx);
    strapp(buffer, "\tcltq\n");
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tmovl -%i(%%rbp,%%r%s,4), %%%sd\n", offset, r_temp, ctx->allocator.scratch[r.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tmovq -%i(%%rbp,%%r%s,4), %%%sd\n", offset, r_temp, ctx->allocator.scratch[r.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void read_ir(char** buffer, size_t address, struct Register r, struct Context* ctx) {
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tleaq -%i(%%rbp), %%%sd\n", address, ctx->allocator.scratch[r.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tleaq -%i(%%rbp), %%%s\n", address, ctx->allocator.scratch[r.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void return_r(char** buffer, struct Register r, struct Context* ctx) {
    switch (r.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tmovl %%%sd, %%eax\n", ctx->allocator.scratch[r.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tmovq %%%s, %%rax\n", ctx->allocator.scratch[r.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void add_rr(char** buffer, struct Register r1, struct Register r2, struct Context* ctx) {
    switch (r1.size) {
        case 1:
        case 4:
            strfmt(buffer, "\taddl %%%sd, %%%sd\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        case 8:
            strfmt(buffer, "\taddq %%%s, %%%s\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void sub_rr(char** buffer, struct Register r1, struct Register r2, struct Context* ctx) {
    switch (r1.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tsubl %%%sd, %%%sd\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tsubq %%%s, %%%s\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void mul_rr(char** buffer, struct Register r1, struct Register r2, struct Context* ctx) {
    switch (r1.size) {
        case 1:
        case 4:
            strfmt(buffer, "\timull %%%sd, %%%sd\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        case 8:
            strfmt(buffer, "\timulq %%%s, %%%s\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

void logical_rr(char** buffer, struct Register r1, struct Register r2, const char* suffix, struct Context* ctx) {
    switch (r1.size) {
        case 1:
        case 4:
            strfmt(buffer, "\tcmpl %%%sd, %%%sd\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            strfmt(buffer, "\tset%s %%%sb\n", suffix, ctx->allocator.scratch[r1.index]);
            strfmt(buffer, "\tmovzbl %%%sb, %%%sd\n", ctx->allocator.scratch[r1.index], ctx->allocator.scratch[r1.index]);
            break;
        
        case 8:
            strfmt(buffer, "\tcmpq %%%s, %%%s\n", ctx->allocator.scratch[r2.index], ctx->allocator.scratch[r1.index]);
            strfmt(buffer, "\tset%s %%%sb\n", suffix, ctx->allocator.scratch[r1.index]);
            strfmt(buffer, "\tmovzbq %%%s, %%%s\n", ctx->allocator.scratch[r1.index], ctx->allocator.scratch[r1.index]);
            break;
        
        default:
            printf("bad register size\n");
            break;
    }
}

#endif