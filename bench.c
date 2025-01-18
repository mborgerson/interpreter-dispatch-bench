#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define XDPRINTF(x, fmt, ...) \
    do { \
        if (x) { \
            fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#ifndef DEBUG
#define DEBUG 0
#endif

#define DPRINTF(fmt, ...) XDPRINTF(DEBUG, fmt, ##__VA_ARGS__)

/*
 * Opcodes
 */

#define X_OPCODES \
    X(OP_ADD) \
    X(OP_SUB) \
    X(OP_MUL) \
    X(OP_DIV) \
    X(OP_HALT)

#define X(op) op,
typedef enum OpCode {
    X_OPCODES
    OP__COUNT
} OpCode;
#undef X

#define X(op) #op,
const char *op_str[] = {
    X_OPCODES
};
#undef X

/*
 * Registers
 */

#define X_REGS \
    X(REG_PC) \
    X(REG_FLAGS) \
    X(REG_ACC)

#define X(reg) reg,
enum Registers {
    X_REGS
    REG__COUNT
};
#undef X

#define X(reg) #reg,
const char *reg_str[] = {
    X_REGS
};
#undef X

enum FLAGS {
    FLAG_HALTED = 1,
};

typedef struct Instruction {
    uint8_t op;
    uint8_t args[3];
} Instruction;

typedef struct MachineState {
    int regs[REG__COUNT];
} MachineState;

Instruction insns[100000];

// static void debug_pre_op(MachineState *state, Instruction *insn)
// {
//     DPRINTF("@%04d --------", state->regs[REG_PC]);
//     DPRINTF("(reg[%s]: %d)", reg_str[insn->args[0]], state->regs[insn->args[0]]);
//     DPRINTF("(reg[%s]: %d)", reg_str[insn->args[1]], state->regs[insn->args[1]]);
//     DPRINTF("reg[%s] = %s(reg[%s], %d)", reg_str[insn->args[0]], op_str[insn->op],
//                                          reg_str[insn->args[1]], insn->args[2]);
// }

// static void debug_post_op(MachineState *state, Instruction *insn)
// {
//     DPRINTF("(reg[%s]: %d)", reg_str[insn->args[0]], state->regs[insn->args[0]]);
// }

static void impl_OP_ADD(MachineState *state, const Instruction *insn)
{
    state->regs[insn->args[0]] = state->regs[insn->args[1]] + insn->args[2];
    state->regs[REG_PC] += 1;
}

static void impl_OP_SUB(MachineState *state, const Instruction *insn)
{
    state->regs[insn->args[0]] = state->regs[insn->args[1]] - insn->args[2];
    state->regs[REG_PC] += 1;
}

static void impl_OP_MUL(MachineState *state, const Instruction *insn)
{
    state->regs[insn->args[0]] = state->regs[insn->args[1]] * insn->args[2];
    state->regs[REG_PC] += 1;
}

static void impl_OP_DIV(MachineState *state, const Instruction *insn)
{
    state->regs[insn->args[0]] = state->regs[insn->args[1]] / insn->args[2];
    state->regs[REG_PC] += 1;
}

static void impl_OP_HALT(MachineState *state, const Instruction *insn)
{
    state->regs[REG_FLAGS] |= FLAG_HALTED;
    state->regs[REG_PC] += 1;
}

static int should_run(MachineState *state)
{
    return !(state->regs[REG_FLAGS] & FLAG_HALTED);
}

static void init_insns(void)
{
    srand(1337);
    for (int i = 0; i < ARRAY_SIZE(insns); i++) {
        int op, val;

        if (i == (ARRAY_SIZE(insns) - 1)) {
            op = OP_HALT;
            val = 0;
        } else {
            op = rand() % (OP__COUNT - 1);
            val = rand() % 10;
            if (op == OP_DIV && val == 0) {
                val = 1;
            }
        }
        insns[i] = (Instruction){
            .op = op,
            .args = { REG_ACC, REG_ACC, val },
        };
    }
}

static void validate_insn(Instruction *insn)
{
    assert(insn->op < OP__COUNT);
    assert(insn->args[0] < REG__COUNT);
    assert(insn->args[1] < REG__COUNT);
}

static void validate_insns(void)
{
    for (int i = 0; i < ARRAY_SIZE(insns); i++) {
        validate_insn(&insns[i]);
    }
}

#define X_METHODS \
    X(switch) \
    X(goto) \
    X(tail)

#define X(m) void exec_ ## m ## _interp(MachineState *state);
X_METHODS
#undef X

#define NUM_ITERATIONS 1000

typedef struct Method {
    const char *name;
    void (*handler)(MachineState *state);
} Method;

const Method methods[] = {
#define X(m) { #m, exec_ ## m ## _interp },
X_METHODS
#undef X
};

static int compare_ints(const void *a, const void *b)
{
    return *(int*)a - *(int*)b;
}

int main(int argc, char const *argv[])
{
    init_insns();
    validate_insns();

    for (int method_idx = 0; method_idx < ARRAY_SIZE(methods); method_idx++) {
        const Method * const method = &methods[method_idx];  
        printf("[%6s] ", method->name);

        int samples[NUM_ITERATIONS];
        int sum = 0;
        int result;

        for (int iter = 0; iter < NUM_ITERATIONS; iter++ ) {
            struct timespec start, end;
            MachineState state;
            memset(&state, 0, sizeof(state));
            clock_gettime(CLOCK_MONOTONIC, &start);
            method->handler(&state);
            clock_gettime(CLOCK_MONOTONIC, &end);

            if (iter == 0) {
                result = state.regs[REG_ACC];
            } else {
                assert(result == state.regs[REG_ACC]);
            }

            assert(state.regs[REG_PC] == ARRAY_SIZE(insns));
            assert(state.regs[REG_FLAGS] & FLAG_HALTED);

            uint64_t start_ns = (uint64_t)start.tv_sec * (uint64_t)1000000000 + start.tv_nsec;
            uint64_t end_ns   = (uint64_t)end.tv_sec   * (uint64_t)1000000000 + end.tv_nsec;

            samples[iter] = (end_ns - start_ns) / 1000;
            sum += samples[iter];
        }
        
        qsort(samples, ARRAY_SIZE(samples), sizeof(samples[0]), compare_ints);

        int min = samples[0],
            max = samples[ARRAY_SIZE(samples) - 1],
            avg = sum / ARRAY_SIZE(samples),
            med = samples[ARRAY_SIZE(samples) / 2];
        printf("min: %d us, max: %d us, avg: %d us, med: %d us\n",
                min, max, avg, med);
    }

    return 0;
}

/*
 * Benchmark basic switch interpreter
 */

void exec_switch_interp(MachineState *state)
{
    while (should_run(state)) {
        Instruction *insn = &insns[state->regs[REG_PC]];

        switch (insn->op) {
        #define X(op) \
        case op: \
            impl_ ## op (state, insn); \
            break;
        X_OPCODES
        #undef X
        }
    }
}

/*
 * Benchmark computed goto interpreter
 */

void exec_goto_interp(MachineState *state)
{
    Instruction *insn;
    
    #define OP_CASE(op) CASE_ ## op

    const void *goto_dispatch_tbl[] = {
#define X(op) &&OP_CASE(op),
X_OPCODES
#undef X
    };

    while (1) {
    #define DISPATCH() \
        if (!should_run(state)) { \
            break; \
        } \
        insn = &insns[state->regs[REG_PC]]; \
        goto *goto_dispatch_tbl[insn->op];

        DISPATCH()
    #define X(op) \
    OP_CASE(op): \
        impl_ ## op (state, insn); \
        DISPATCH()
    X_OPCODES
    }
    #undef X
    #undef DISPATCH
    #undef OP_CASE
}

/*
 * Benchmark tail call interpreter
 */

#define MUST_TAIL [[clang::musttail]]
#define PRESERVE_NONE [[clang::preserve_none]]

static void exec_tail_dispatch(MachineState *state, const Instruction *insn) PRESERVE_NONE;

#define HANDLER(op) exec_tail_ ## op
#define X(op) \
static void HANDLER(op)(MachineState *state, const Instruction *insn) PRESERVE_NONE \
{ \
    impl_ ## op (state, insn); \
    MUST_TAIL return exec_tail_dispatch(state, insn); \
}
X_OPCODES
#undef X

typedef void (*tail_dispatch_op_handler)(MachineState *state, const Instruction *insn) PRESERVE_NONE;
static const tail_dispatch_op_handler tail_dispatch_tbl[] = {
#define X(op) exec_tail_ ## op,
X_OPCODES
#undef X
};

static void exec_tail_dispatch(MachineState *state, const Instruction *insn) PRESERVE_NONE
{
    if (!should_run(state)) {
        return;
    }
    insn = &insns[state->regs[REG_PC]];
    MUST_TAIL return tail_dispatch_tbl[insn->op](state, insn);
}

void exec_tail_interp(MachineState *state)
{
    exec_tail_dispatch(state, NULL);
}
