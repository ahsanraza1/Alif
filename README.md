# ALIF

ALIF is a custom **register-based virtual machine** written in pure C. The machine is 32-bit: every instruction is one 32-bit word, eight integer registers (`R1`..`R8`), and a **1 KiB** data RAM. The host program **`alif`** loads a `.afbin` binary and runs it.

## Documents

| Document | Audience | Contents |
|---|---|---|
| [`docs/ISA.md`](docs/ISA.md) | ISA / compiler / assembler authors | Instruction formats, `.afbin` layout, every opcode, flags, faults |
| [`docs/IMPLEMENTATION.md`](docs/IMPLEMENTATION.md) | VM implementers | Engine loop, `.afbin` loader, bounds checks |
| [`include/opcodes.h`](include/opcodes.h) | C sources | Raw `#define` opcode / register / field values |
| [`include/alif.h`](include/alif.h) | C sources | `struct alif_vm`, `alif_exec` / `alif_exec_from` |
| [`include/alf.h`](include/alf.h) | C sources | `.afbin` image load/write |
| [`src/vm.c`](src/vm.c) | C sources | Execution engine |
| [`src/load.c`](src/load.c) | C sources | `.afbin` file I/O |
| [`src/alif.c`](src/alif.c) | C sources | Launcher (`alif`) — runs `.afbin` only |
| [`afas/`](afas/) | optional assembler | **`afas`**: `.afb` assembly → `.afbin` |
| [`examples/`](examples/) | programs | `.afb` sources and `.afbin` images |
| [`tests/smoke.c`](tests/smoke.c) | bring-up | ALU, RAM, and fault-class checks |

## Build and run

You need **gcc**. You do **not** need `make`. From the repo root:

```
build.bat
```

That produces `alif.exe` and `afas\afas.exe`. Same thing by hand:

```
gcc -std=c11 -Wall -Wextra -Werror -I include -o alif.exe src/alif.c src/load.c src/vm.c
gcc -std=c11 -Wall -Wextra -Werror -I include -o afas/afas.exe afas/afas.c src/load.c
```

Only the VM launcher:

```
build.bat alif
```

Only the assembler:

```
build.bat afas
```

Then:

```
alif.exe examples/add.afbin
```

Assemble source, then run (two separate programs):

```
afas\afas.exe examples/add.afb
alif.exe examples/add.afbin
```

```
programmer  →  program.afb  →  afas.exe  →  program.afbin  →  alif.exe  →  VM
```

`examples/add.afbin` prints `5`. `examples/hello.afbin` prints `ALIF`. Assembly language: [`afas/README.md`](afas/README.md). Binary layout: [`examples/README.md`](examples/README.md).

**`alif` pipeline:** `.afbin` → VM. **`afas` pipeline (optional):** `.afb` → `.afbin`. `alif` never reads `.afb`.

A `Makefile` is in the tree for people who have GNU make. It is not required.

Usage: `alif <program.afbin>` — the path must end in `.afbin`.

| Exit | Meaning |
|---|---|
| 0 | `HLT` or `TRAP 0` |
| 1 | bad arguments, missing file, or invalid `.afbin` |
| 2 | VM fault (message on stderr) |
| 1–255 | `TRAP` imm16 when nonzero |

A `.afbin` file is **not** C source. It is a 32-byte header (`ALIF` magic, version 1.0, entry IP, sizes) plus little-endian instruction words, then an optional RAM init blob. Layout: [`docs/ISA.md`](docs/ISA.md) §10.2.

Host binaries (`alif.exe`, test helpers, `*.o`) are listed in [`.gitignore`](.gitignore).

## Engine API (no files)

The VM itself still only accepts a byte buffer. The launcher is what reads the disk.

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
- **IP:** `vm.ip` is a byte offset into `code`.
- Bounds: fetch, RAM, stack, register ids, divide-by-zero, and a 1 000 000-step cap.

## Numeric contract (summary)

- **Word:** 32 bits. **Instruction size:** 4 bytes, naturally aligned, little-endian in the payload.
- **GPRs:** `R1`..`R8` encoded as ids `0`..`7`.
- **Opcode:** bits `[31:24]`. High nibble is the class (`0x1_` arithmetic, `0x5_` control flow, …).
- **Hidden state:** `IP`, `SP`, `FLAGS` — not addressable as GPRs.

Do not invent parallel enum wrappers for opcodes. Include `opcodes.h`.
