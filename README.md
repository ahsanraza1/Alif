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
| [`afas/`](afas/) | optional assembler | **`afas`**: `.afb` → `.afbin` |
| [`alifc/`](alifc/) | optional compiler | **`alifc`**: `.alif` (UTF-8 or AF8) → `.afb` |
| [`af8/`](af8/) | compiler encoding | **AF8**: 8-bit Urdu codes for `.alif` source (not used by the VM) |
| [`lang/`](lang/) | source language | **علیف** grammar (`alifc` compiles it) |
| [`web/`](web/) | tutorial | Static HTML handbook for علیف v1 (`web/index.html`) |
| [`examples/`](examples/) | programs | `.afb` sources and `.afbin` images |
| [`tests/smoke.c`](tests/smoke.c) | bring-up | ALU, RAM, and fault-class checks |

## Build and run

You need **gcc**. You do **not** need `make`. From the repo root:

```
build.bat
```

That produces `alif.exe`, `afas\afas.exe`, and `alifc\alifc.exe`, and copies them into **`ship\`** (gitignored) with `run.bat`. Same thing by hand:

```
gcc -std=c11 -Wall -Wextra -Werror -I include -o alif.exe src/alif.c src/load.c src/vm.c
gcc -std=c11 -Wall -Wextra -Werror -I include -o afas/afas.exe afas/afas.c src/load.c
gcc -std=c11 -Wall -Wextra -Werror -o alifc/alifc.exe alifc/alifc.c
```

Only the VM launcher:

```
build.bat alif
```

Only the assembler:

```
build.bat afas
```

Only the compiler:

```
build.bat alifc
```

Ship folder (the three `.exe` files plus `run.bat`):

```
ship\run.bat alifc\samples\add.alif
```

Copy `ship\` to someone else; they only need that folder. `run.bat` calls `alifc.exe`, then `afas.exe`, then `alif.exe`. The tools stay separate.

Then:

```
alif.exe examples/add.afbin
```

Urdu source → assembly → binary → VM (three programs):

```
alifc\alifc.exe alifc\samples\add.alif
afas\afas.exe alifc\samples\add.afb
alif.exe alifc\samples\add.afbin
```

```
programmer  →  .alif  →  alifc  →  .afb  →  afas  →  .afbin  →  alif.exe  →  VM
```

`examples/add.afbin` prints `8` (`2 + 6`). `alifc\samples\add.afbin` prints the same from Urdu source. `examples/hello.afbin` prints `ALIF`. Assembly language: [`afas/README.md`](afas/README.md). Binary layout: [`examples/README.md`](examples/README.md).

**`alif` pipeline:** `.afbin` → VM. **`afas`:** `.afb` → `.afbin`. **`alifc`:** `.alif` → `.afb`. Each tool is optional for the others.

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

## Source language

**علیف** (`lang/`): Urdu keywords, ASCII operators. Write `.alif` as **UTF-8** in an editor (`alifc` transcodes to AF8). Grammar: [`lang/README.md`](lang/README.md). Encoding: [`af8/README.md`](af8/README.md). Compiler **`alifc`**: `.alif` → `.afb` ([`alifc/README.md`](alifc/README.md)). Then `afas` and `alif` as before.

