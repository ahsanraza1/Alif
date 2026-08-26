# ALIF

ALIF is a custom **register-based virtual machine** written in pure C. The machine is 32-bit: every instruction is one 32-bit word, eight integer registers (`R1`..`R8`), and a **1 KiB** data RAM. Instructions are fetched from a caller-supplied `unsigned char` bytecode payload via an instruction pointer (`ip`).

## Documents

| Document | Audience | Contents |
|---|---|---|
| [`docs/ISA.md`](docs/ISA.md) | ISA / compiler / assembler authors | Instruction formats, register file, every opcode, flags, faults |
| [`docs/IMPLEMENTATION.md`](docs/IMPLEMENTATION.md) | VM implementers | Fetch-decode-execute loop, bounds checks, Harvard split |
| [`include/opcodes.h`](include/opcodes.h) | C sources | Raw `#define` opcode / register / field values |
| [`include/alif.h`](include/alif.h) | C sources | `struct alif_vm`, `alif_exec`, RAM size, fault codes |
| [`src/vm.c`](src/vm.c) | C sources | Execution engine |
| [`tests/smoke.c`](tests/smoke.c) | bring-up | ALU, RAM, and fault-class checks |

Read **ISA.md** first if you emit ALIF words. Read **IMPLEMENTATION.md** first if you are changing the interpreter.

## Run the smoke test

```
cc -I include -o tests/smoke tests/smoke.c src/vm.c
./tests/smoke
```

Expected: a line `5` on stdout (`OUT 1` of `2+3`) and `ok` on stderr.

## Engine API

```c
#include "alif.h"

struct alif_vm vm;
unsigned char code[] = { /* little-endian instruction words */ };

alif_vm_init(&vm);
int rc = alif_exec(&vm, code, sizeof code);
/* rc == 0 → HLT/TRAP; nonzero → ALIF_FAULT_* */
```

- **Registers:** `vm.regs[R1]` … `vm.regs[R8]` (`int[8]`).
- **RAM:** `vm.ram[1024]`. Stack grows down from address 1024.
- **IP:** `vm.ip` is a byte offset into `code`, advanced by 4 unless a jump/`HLT`/`TRAP` says otherwise.
- Bounds: fetch, RAM, stack, register ids, divide-by-zero, and a 1 000 000-step cap. The loop will not read or write outside those arrays.

## Numeric contract (summary)

- **Word:** 32 bits. **Instruction size:** 4 bytes, naturally aligned, little-endian in the payload.
- **GPRs:** `R1`..`R8` encoded as ids `0`..`7`.
- **Opcode:** bits `[31:24]`. High nibble is the class (`0x1_` arithmetic, `0x5_` control flow, …).
- **Hidden state:** `IP`, `SP`, `FLAGS` — not addressable as GPRs.

Do not invent parallel enum wrappers for opcodes. Include `opcodes.h`.
