# ALIF

ALIF is a custom **register-based virtual machine** written in pure C. The machine is 32-bit: every instruction is one 32-bit word, every general-purpose register holds 32 bits, and the default memory access size is one word.

This repository currently defines the **ISA numeric contract** — opcodes, registers, instruction-field layout, and flag bits. The interpreter, assembler, and memory image are not in-tree yet.

## Documents

| Document | Audience | Contents |
|---|---|---|
| [`docs/ISA.md`](docs/ISA.md) | ISA / compiler / assembler authors | Instruction formats, register file, every opcode, flags, exceptions |
| [`docs/IMPLEMENTATION.md`](docs/IMPLEMENTATION.md) | VM implementers | Decode formulas, host endianness, memory model, fetch-execute loop, binary image |
| [`include/opcodes.h`](include/opcodes.h) | C sources | Raw `#define` values only — the single source of numeric truth |

Read **ISA.md** first if you are designing an assembler, compiler backend, or another tool that *emits* ALIF code. Read **IMPLEMENTATION.md** first if you are writing the C interpreter.

## Numeric contract (summary)

- **Word:** 32 bits. **Instruction size:** 4 bytes, naturally aligned.
- **GPRs:** `R1`..`R8` encoded as ids `0`..`7` (see why in ISA.md).
- **Opcode:** bits `[31:24]`. High nibble is the class (`0x1_` arithmetic, `0x5_` control flow, …).
- **Hidden state:** `PC`, `SP`, `FLAGS` — not addressable as GPRs.

```
#include "opcodes.h"

uint32_t insn = /* fetched word */;
uint8_t  op   = (insn >> OP_SHIFT) & OP_MASK;
uint8_t  rd   = (insn >> RD_SHIFT) & REG_MASK;
```

Do not invent parallel enum wrappers. Tools must include `opcodes.h` (or regenerate from it) so opcode numbers cannot drift.
