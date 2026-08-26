# ALIF implementation notes

This is the engineer’s companion to [`ISA.md`](ISA.md). It explains how to build the C interpreter against `include/opcodes.h` without inventing a second contract.

---

## 1. Recommended C types

```c
#include <stdint.h>
#include "opcodes.h"

typedef uint32_t alif_word;     /* GPR, memory word, instruction */
typedef uint32_t alif_addr;     /* byte address                   */
typedef uint8_t  alif_op;       /* opcode                         */
typedef uint8_t  alif_reg;      /* 0..7                           */
```

Use `uint32_t` for all machine words so host `int` width cannot leak in. For signed operations, cast explicitly:

```c
int32_t s = (int32_t)a - (int32_t)b;          /* SUB / CMP, OF/SF */
uint32_t u = a - b;                            /* same bits, CF   */
alif_word arith_right = (uint32_t)((int32_t)val >> count);
```

Do **not** shift a 32-bit value by 32 on the host (`<< 32` is undefined in C). The ISA already masks shift counts with `& 31`.

---

## 2. Core struct

Hidden architectural state from ISA §2:

```c
struct alif_vm {
    alif_word  r[ALIF_NREGS];   /* r[0] is R1, r[7] is R8 */
    alif_word  pc;
    alif_word  sp;
    alif_word  flags;
    uint8_t   *mem;
    size_t     mem_size;
    int        halt;            /* 1 after HLT or fault */
    int        exit_code;
};
```

Index GPRs with the **encoded id**, not the human number:

```c
vm->r[R1]  /* first register, id 0 */
vm->r[R8]  /* last  register, id 7 */
```

A common bug is `r[1]`..`r[8]` with a 9-slot array. That disagrees with `opcodes.h`. Use `r[ALIF_NREGS]` and `R1`..`R8` as indices.

---

## 3. Endianness

Two different orders exist. Mixing them is the usual first bug.

**Architectural bit order** (diagrams, `OP_SHIFT`): bit 31 is the opcode MSB. A word with opcode `OP_HLT` (`0xFF`) and zeros elsewhere is the integer `0xFF000000`.

**Memory / file byte order:** little-endian. The same `HLT` is stored as four bytes:

```
offset+0  00
offset+1  00
offset+2  00
offset+3  FF
```

Fetch must assemble a host `uint32_t` from those bytes, not by casting a `uint32_t *` on a big-endian host.

Portable fetch:

```c
static alif_word load32(const uint8_t *p)
{
    return  (alif_word)p[0]
         | ((alif_word)p[1] << 8)
         | ((alif_word)p[2] << 16)
         | ((alif_word)p[3] << 24);
}

static void store32(uint8_t *p, alif_word w)
{
    p[0] = (uint8_t)(w      );
    p[1] = (uint8_t)(w >>  8);
    p[2] = (uint8_t)(w >> 16);
    p[3] = (uint8_t)(w >> 24);
}
```

On a little-endian host, `memcpy` of 4 bytes yields the same integer. Prefer `load32`/`store32` so the interpreter is host-endian safe.

---

## 4. Decode

```c
alif_word insn = load32(vm->mem + vm->pc);

alif_op   op   = (alif_op)  ((insn >> OP_SHIFT)  & OP_MASK);
alif_reg  rd   = (alif_reg) ((insn >> RD_SHIFT)  & REG_MASK);
alif_reg  rs1  = (alif_reg) ((insn >> RS1_SHIFT) & REG_MASK);
alif_reg  rs2  = (alif_reg) ((insn >> RS2_SHIFT) & REG_MASK);
uint32_t  imm12 =            insn                & IMM12_MASK;
uint32_t  imm16 =            insn                & IMM16_MASK;
uint32_t  imm24 =            insn                & IMM24_MASK;
```

Sign-extend helpers:

```c
static alif_word sext12(uint32_t x)
{
    return (alif_word)((int32_t)(x << 20) >> 20);
}
static alif_word sext16(uint32_t x)
{
    return (alif_word)((int32_t)(x << 16) >> 16);
}
```

**Register-id check** before any `vm->r[rd]` access:

```c
if (rd >= ALIF_NREGS || rs1 >= ALIF_NREGS || rs2 >= ALIF_NREGS)
    fault(vm, FAULT_ILL);
```

J-type instructions do not use `rd`/`rs1`/`rs2`; skip the check for unused fields or require them to be zero (ISA recommends zero; trapping on non-zero catches assembler bugs early).

**Class dispatch** (optional fast path):

```c
switch (op >> 4) {
case 0x0: /* movement */ break;
case 0x1: /* arithmetic */ break;
case 0x2: /* logic */ break;
case 0x3: /* shift */ break;
case 0x4: /* compare */ break;
case 0x5: /* jump */ break;
case 0x6: /* stack */ break;
case 0x8: /* I/O */ break;
case 0xF: /* system */ break;
default:  fault(vm, FAULT_ILL);
}
```

Then switch on the exact `op`. Unknown members of a defined class (`0x06`, `0x1A`, `0x5B`, …) are still illegal.

---

## 5. Fetch-execute loop

```
halt = 0
while !halt:
    if pc & 3 != 0:           FAULT_ALIGN
    if pc + 4 > code_end:     FAULT_MEM   (or ILL, pick one)
    insn = load32(mem + pc)
    decode fields
    execute
    if instruction did not write pc:
        pc = pc + 4
```

`HLT` sets `halt = 1` and **does not** advance `PC` (ISA §7.9).

`JMP`/`CALL`/taken `Jcc`/`RET` write `PC` themselves. Use a local `int branched = 0` so the loop does not double-add 4.

```c
int branched = 0;
switch (op) {
case OP_JMP:
    vm->pc = imm24;
    branched = 1;
    break;
/* ... */
}
if (!vm->halt && !branched)
    vm->pc += ALIF_INSN_BYTES;
```

---

## 6. FLAGS helpers

Bit macros are in `opcodes.h`: `FLAG_CF`, `FLAG_ZF`, `FLAG_SF`, `FLAG_OF`.

```c
static void set_zf_sf(struct alif_vm *vm, alif_word res)
{
    vm->flags &= ~(FLAG_ZF | FLAG_SF);
    if (res == 0)           vm->flags |= FLAG_ZF;
    if (res & 0x80000000u)  vm->flags |= FLAG_SF;
}

static int add_of(alif_word a, alif_word b, alif_word res)
{
    /* signed overflow: a and b same sign, res opposite */
    return ((~(a ^ b) & (a ^ res)) & 0x80000000u) != 0;
}

static int sub_of(alif_word a, alif_word b, alif_word res)
{
    /* signed overflow: a and b opposite sign, res opposite to a */
    return (((a ^ b) & (a ^ res)) & 0x80000000u) != 0;
}
```

Carry for add: `res < a` (unsigned). Borrow for sub: `a < b` (unsigned).

`Jcc` predicates:

```c
int zf = (vm->flags & FLAG_ZF) != 0;
int sf = (vm->flags & FLAG_SF) != 0;
int of = (vm->flags & FLAG_OF) != 0;

case OP_JZ:  case OP_JE:  take = zf;           break;
case OP_JNZ: case OP_JNE: take = !zf;          break;
case OP_JL:               take = sf != of;     break;
case OP_JLE:              take = zf || sf != of; break;
case OP_JG:               take = !zf && sf == of; break;
case OP_JGE:              take = sf == of;     break;
```

`OP_JE` must share the `OP_JZ` arm (or fall through). Same for `OP_JNE` / `OP_JNZ`.

---

## 7. Memory and stack

Allocate one host buffer `mem[0 .. mem_size)`. Map VM address `A` to `mem + A`. Reject any access where `A + 4 > mem_size`.

Alignment: `if (addr & 3) fault(ALIGN)` before `load32`/`store32`.

Stack reset (empty stack, grows down):

```c
vm->sp = stack_base + stack_size;   /* one past last stack byte, 4-aligned */
```

`PUSH` / `CALL`:

```c
if (vm->sp < 4) fault(STK);
vm->sp -= 4;
if (vm->sp < stack_base) fault(STK);
store32(vm->mem + vm->sp, value);
```

`POP` / `RET`:

```c
if (vm->sp + 4 > stack_base + stack_size) fault(STK);
value = load32(vm->mem + vm->sp);
vm->sp += 4;
```

`CALL` pushes `pc + 4` **before** overwriting `pc`. Compute `ret = vm->pc + 4` first.

---

## 8. I/O host binding

Keep port handlers out of the ALU switch. A small table is enough:

```c
case OP_OUT:
    switch (imm16) {
    case 0: putchar((int)(vm->r[rs1] & 0xFF)); fflush(stdout); break;
    case 1: printf("%u\n", (unsigned)vm->r[rs1]); break;
    case 2: printf("%08X\n", (unsigned)vm->r[rs1]); break;
    default: fault(vm, FAULT_IO);
    }
    break;
```

`IN` port 0: `int c = getchar(); rd = (c == EOF) ? 0xFFFFFFFF : (uint8_t)c`.

Do not implement network or filesystem ports in v1.

---

## 9. File layout for sources (suggested)

```
Alif/
  include/opcodes.h          numeric contract (exists)
  docs/ISA.md                behaviour contract (exists)
  docs/IMPLEMENTATION.md     this file
  src/vm.c                   fetch-execute
  src/mem.c                  load32/store32, image loader
  src/exec.c                 per-opcode bodies (optional split)
  tools/as.c                 assembler later
  tests/...                  binary snippets + expected stdout
```

`opcodes.h` must remain **defines only**. No `static inline` encoders, no `enum`, no `typedef` in that header. Encoders live in the assembler.

Include path: `-I include` so sources write `#include "opcodes.h"`.

---

## 10. Assembler notes (when you write one)

Mnemonic form used in ISA examples:

```
MOV   rd, rs1
MOVI  rd, imm16
ADD   rd, rs1, rs2
ADDI  rd, rs1, imm16
LOAD  rd, [rs1+imm12]
STORE rd, [rs1+imm12]
JMP   abs24
OUT   port, rs1
IN    rd, port
PUSH  rs1
POP   rd
HLT
```

Register tokens: `R1`..`R8` only. Map token `R1` → encoding `0`, not `1`.

Immediate ranges to check at assemble time:

| Field | Valid range |
|---|---|
| `imm12` | `-2048` .. `2047` |
| `imm16` signed | `-32768` .. `32767` |
| `imm16` unsigned (`MOVI`, ports, trap) | `0` .. `65535` |
| `imm24` | `0` .. `0xFFFFFF`, multiple of 4 |

Reject `R9`, `R0`, and lowercase-only variants unless you explicitly accept `r1` as alias of `R1`.

---

## 11. Test vectors

Minimum smoke tests after the interpreter exists:

1. `NOP; HLT` — exits 0, `PC` on `HLT`.
2. `MOVI R1, 2; MOVI R2, 3; ADD R1, R1, R2; OUT 1, R1; HLT` — prints `5`.
3. `MOVI R1, 1; CMP R1, R1; JE <hlt>; TRAP 1` — takes `JE`.
4. `DIV R1, R1, R2` with `R2 = 0` — `FAULT_DIV0`.
5. `LOAD` from address `1` — `FAULT_ALIGN`.
6. Opcode `0x70` — `FAULT_ILL`.
7. Register field `rd = 8` — `FAULT_ILL`.
8. `PUSH R1; POP R2` — `R2 == original R1`, `SP` restored.

Keep tests as raw word arrays (or the `.alif` image from ISA §10) so they do not depend on an assembler.

---

## 12. Performance (not required for v1)

The encoding is already decoder-friendly:

- One load per instruction.
- Opcode in the high byte: on LE hosts that byte is `mem[pc+3]`; you may fetch opcode first for a tracing JIT later, but the interpreter should still use `load32` so behaviour stays portable.
- Eight GPRs fit in registers on the host; keep `struct alif_vm` pointer in a local and hope the compiler caches `r[]`.
- Do not thread-code until the opcode set is stable.

---

## 13. Checklist for a first `vm.c`

- [ ] Include `opcodes.h`; no duplicate `#define OP_*`.
- [ ] `r[8]`, indexed by `R1`..`R8`.
- [ ] LE `load32`/`store32`.
- [ ] `pc += 4` only when the op did not branch and is not `HLT`.
- [ ] FLAGS on ALU/CMP; `Jcc` reads FLAGS, does not write them.
- [ ] `JE`/`JZ` identical; `JNE`/`JNZ` identical.
- [ ] Divide by zero and illegal opcode fault, no wrap-around.
- [ ] `OUT 1` bound to stdout for bring-up.
- [ ] Stack grows down; `CALL`/`RET` use the same `SP` as `PUSH`/`POP`.
