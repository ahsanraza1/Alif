# ALIF implementation notes

Companion to [`ISA.md`](ISA.md). Describes the C engine that actually ships: `include/alif.h` + `src/vm.c`, plus the `.afbin` loader/launcher.

---

## 1. Files

```
include/opcodes.h    numeric ISA (defines only)
include/alif.h       struct alif_vm, ALIF_RAM_SIZE, fault codes, API
include/alf.h        .afbin on-disk header, alif_load_afbin / alif_write_afbin
src/vm.c             fetch-decode-execute loop
src/load.c           .afbin reader/writer
src/alif.c           launcher main (binary name: alif)
examples/*.afbin     programs (the only files alif runs)
examples/README.md   how to write a .afbin by hand
tests/smoke.c        bring-up: ALU, RAM, and every fault class
tests/mk_examples.c  writes examples/add.afbin and examples/hello.afbin
Makefile             builds alif
.gitignore           host binaries, *.exe, retired *.alf
```

Build (from the repo root):

```
make            # gcc -o alif src/alif.c src/load.c src/vm.c
make examples   # emit examples/*.afbin
make smoke      # engine tests + alif examples/add.afbin and hello.afbin
```

Windows without make:

```
gcc -std=c11 -Wall -Wextra -Werror -I include -o alif src/alif.c src/load.c src/vm.c
```

`opcodes.h` stays **defines only**. The engine is ordinary C in `vm.c`. The launcher is the only program that talks to the filesystem.

---

## 2. Machine state

```c
struct alif_vm {
    int            regs[ALIF_NREGS];    /* integer array, R1..R8 at 0..7 */
    unsigned char  ram[ALIF_RAM_SIZE];  /* 1 KiB virtual RAM             */
    unsigned int   ip;                  /* byte index into bytecode      */
    unsigned int   sp;                  /* byte address in ram[]         */
    unsigned int   flags;
    int            halt;
    int            fault;
    unsigned int   trap_code;
    unsigned int   steps;
};
```

Harvard split:

- **Bytecode** is the `const unsigned char *code` argument. `ip` fetches four bytes from it. The engine never writes this buffer.
- **RAM** is `ram[1024]`. `LOAD`/`STORE`/`PUSH`/`POP`/`CALL`/`RET` use it. The engine never fetches instructions from RAM.

Index GPRs with encodings, not human numbers: `vm->regs[R1]` is the first register (`id 0`). Do not allocate `regs[9]` and skip slot 0.

`alif_vm_init` zeros the whole struct then sets `sp = 1024`. `alif_exec` resets `ip`, `halt`, `fault`, `trap_code`, `steps`; it does **not** clear `regs`/`ram`/`flags`/`sp`, so a caller can preload RAM. For a cold run, call `init` then `exec`.

---

## 3. The loop

`alif_exec(vm, code, code_len)`:

1. Reject `vm == NULL` (`ILL`), `code == NULL` or `code_len < 4` (`MEM`).
2. While `!vm->halt`:
   - If `steps >= ALIF_MAX_STEPS` (1 000 000) → `FAULT_STEP`.
   - **Fetch** four bytes at `ip` (see §4).
   - **Decode** `op, rd, rs1, rs2, imm12, imm16, imm24` with the shifts in `opcodes.h`.
   - **Execute** one `switch (op)` arm. Unknown op → `FAULT_ILL`.
   - If the op did not halt and did not branch: `ip += 4`, after checking `ip` will not wrap `unsigned int`.
3. Return `vm->fault` (`0` on `HLT`/`TRAP`).

`HLT` and `TRAP` set `halt = 1` and `branched = 1` so `ip` stays on that word.

---

## 4. Byte fetch (instruction pointer)

Fetch is byte-wise, little-endian, **fail closed**:

```
if code == NULL            → MEM
if ip & 3                  → ALIGN
if ip >= code_len          → MEM
if code_len - ip < 4       → MEM     (no ip+4 overflow on size_t)
word = code[ip]
     | code[ip+1] << 8
     | code[ip+2] << 16
     | code[ip+3] << 24
```

The architectural integer `0xFF000000` (`HLT`) is therefore payload bytes `00 00 00 FF`.

Jumps write `ip = imm24` only after `jump_ok`:

- `imm24` 4-aligned
- `imm24 + 4 <= code_len`

A jump never uses RAM addresses. `RET` pops a payload offset and runs the same check.

---

## 5. RAM bounds

Every RAM access goes through `ram_load` / `ram_store` / `ea_ram`.

Effective address for `LOAD`/`STORE`:

```
ea = (int64_t)(uint32_t)regs[rs1] + (int32_t)sext12(imm12)
```

Reject if `ea < 0` or `ea > 1020` (`MEM`) or `ea & 3` (`ALIGN`). The 64-bit sum stops a 32-bit wrap from looking like a legal address inside 1 KiB.

`ram_store` writes exactly four bytes `addr .. addr+3`. There is no path that indexes `ram[1024]`.

Stack (grows down, empty `sp = 1024`):

```
PUSH / CALL:  if sp < 4 or sp > 1024 → STK;  sp -= 4; store at sp
POP  / RET:   if sp > 1020 → STK;  load at sp;  sp += 4;  if sp > 1024 → STK
```

---

## 6. Register bounds

Any used `rd` / `rs1` / `rs2` must be `< ALIF_NREGS` (8). Encoding `8..15` → `FAULT_ILL` before `regs[]` is indexed. J-type does not consult those fields.

---

## 7. FLAGS (as implemented)

Helpers in `vm.c`: `flags_add`, `flags_sub`, `flags_logic`, `flags_zsf`.

`JE` shares the `JZ` condition; `JNE` shares `JNZ`. Conditions match ISA §6.

Shift count is `rs2 & 31` so the host never shifts by 32 (undefined in C).

---

## 8. I/O

| Port | Op | Host |
|---|---|---|
| 0 | `OUT` | `putchar` of low 8 bits |
| 0 | `IN` | `getchar`; `0xFFFFFFFF` on EOF |
| 1 | `OUT` | `printf("%u\n", rs1)` |
| 2 | `OUT` | `printf("%08X\n", rs1)` |
| other | — | `FAULT_IO` |

---

## 9. Return values

| `alif_exec` | Meaning |
|---|---|
| `ALIF_OK` (0) | `HLT` or `TRAP` (`trap_code` is the `TRAP` imm16, else 0) |
| `1..7` | fault; see ISA §8 and `alif.h` |

On a fault, `regs`/`ram` are left as they were **before** the failing write when the check happens first (jumps, EA, div0). `CALL` validates the target **then** pushes, so a bad target does not move `SP`.

---

## 10. Encoding a word in C

```c
uint32_t word = ((uint32_t)op << OP_SHIFT)
              | ((uint32_t)rd << RD_SHIFT)
              | ((uint32_t)rs1 << RS1_SHIFT)
              | ((uint32_t)rs2 << RS2_SHIFT)
              | (imm & mask);

payload[i]   = (unsigned char)(word      );
payload[i+1] = (unsigned char)(word >>  8);
payload[i+2] = (unsigned char)(word >> 16);
payload[i+3] = (unsigned char)(word >> 24);
```

Worked values: ISA §9. `tests/smoke.c` encodes the `2+3` program the same way. `tests/mk_examples.c` packs it into `examples/add.afbin` (the file programmers ship).

---

## 11. The `alif` launcher

`src/alif.c` is the host program. It does not decode opcodes; it only:

1. Requires `argv[1]` ending in `.afbin` (usage otherwise).
2. Calls `alif_load_afbin` (`src/load.c`).
3. `alif_vm_init`, copies the data section into `vm.ram`.
4. `alif_exec_from(..., img.entry)`.
5. Maps the result to a process exit code (ISA §10.2).

Loader rejects: path not ending in `.afbin`, missing file, short file, bad magic, version ≠ 1.0, nonzero reserved words, `code_size` not a multiple of 4 or > 16 MiB, `data_size` > 1024, entry not 4-aligned or not inside code, file length ≠ header + code + data, `malloc` failure.

The engine still never `fopen`s. Only the launcher does.

---

## 12. Safety checklist (enforced in `vm.c`)

- [x] No fetch past `code_len`; subtraction form, not `ip + 4` overflow
- [x] No `ram[i]` with `i >= 1024`
- [x] No `regs[i]` with `i >= 8`
- [x] EA computed in `int64_t`
- [x] `sp` cannot decrement below 0 (checked before `sp -= 4`)
- [x] `ip += 4` refused if it would wrap `unsigned int`
- [x] Divide by zero trapped
- [x] Shift count masked to 5 bits
- [x] Step cap against `JMP` to self
- [x] Bytecode buffer is never written

---

## 13. Adding an opcode

1. `#define OP_…` in `opcodes.h`
2. `case OP_…:` in the `switch` in `src/vm.c` with the same bound checks as neighbouring ops
3. Semantics + FLAGS in `docs/ISA.md` §5 and §7
4. A vector in `tests/smoke.c` if the op can fault or write a GPR
