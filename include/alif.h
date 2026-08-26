#ifndef ALIF_H
#define ALIF_H

/*
 * ALIF execution engine — public machine state and entry point.
 *
 * Bytecode is an array of unsigned char (little-endian 32-bit words).
 * Data/stack live in a separate 1 KiB RAM block. The `alif` launcher
 * (`src/alif.c`) loads a `.afbin` file and calls `alif_exec_from`.
 * See docs/ISA.md §10 and docs/IMPLEMENTATION.md.
 */

#include "opcodes.h"

#include <stddef.h>
#include <stdint.h>

#define ALIF_RAM_SIZE           1024
#define ALIF_MAX_STEPS          1000000u

/* alif_exec return / vm->fault */
#define ALIF_OK                 0
#define ALIF_FAULT_ILL          1
#define ALIF_FAULT_ALIGN        2
#define ALIF_FAULT_MEM          3
#define ALIF_FAULT_DIV0         4
#define ALIF_FAULT_STK          5
#define ALIF_FAULT_IO           6
#define ALIF_FAULT_STEP         7

struct alif_vm {
    int             regs[ALIF_NREGS];           /* R1..R8 at indices 0..7 */
    unsigned char   ram[ALIF_RAM_SIZE];         /* 1 KiB virtual RAM      */
    unsigned int    ip;                         /* instruction pointer    */
    unsigned int    sp;                         /* stack pointer (in RAM) */
    unsigned int    flags;
    int             halt;
    int             fault;
    unsigned int    trap_code;                  /* imm16 from OP_TRAP     */
    unsigned int    steps;                      /* instructions retired   */
};

void alif_vm_init(struct alif_vm *vm);

/*
 * Fetch / decode / execute `code[0 .. code_len)`.
 * Returns ALIF_OK on HLT/TRAP, or a FAULT_* code. Never writes outside
 * regs[] / ram[] / the caller’s bytecode buffer (read-only).
 */
int  alif_exec(struct alif_vm *vm, const unsigned char *code, size_t code_len);

/* Same as alif_exec, but start at byte offset `entry` in `code` (must be
 * 4-aligned and inside the payload). Used by the .afbin loader. */
int  alif_exec_from(struct alif_vm *vm, const unsigned char *code, size_t code_len,
                    unsigned int entry);

#endif /* ALIF_H */
