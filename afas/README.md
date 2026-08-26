# afas — ALIF assembler

**afas** (ALIF assembler) is a separate tool. It reads **`.afb`** assembly and writes a **`.afbin`** image. `alif.exe` does not call this program. The VM still only runs `.afbin`.

```
programmer
    |
    |  writes assembly
    v
program.afb
    |
    |  afas
    v
program.afbin
    |
    |  alif.exe   (unchanged)
    v
     VM
```

## Build

You need **gcc**, not `make`. From the **repo root**:

```
build.bat afas
```

or:

```
gcc -std=c11 -Wall -Wextra -Werror -I include -o afas/afas.exe afas/afas.c src/load.c
```

From this folder:

```
gcc -std=c11 -Wall -Wextra -Werror -I ../include -o afas.exe afas.c ../src/load.c
```

`afas` only uses `alif_write_afbin` to emit the existing image format. It does not link or run the VM.

## Run

From the repo root:

```
afas\afas.exe examples/add.afb
alif.exe examples/add.afbin
```

Default output replaces `.afb` with `.afbin` in the same path. Override with `-o`:

```
afas\afas.exe examples/hello.afb -o examples/hello.afbin
```

The source path must end in `.afb`. The output path must end in `.afbin`.

| Exit | Meaning |
|---|---|
| 0 | wrote the image |
| 1 | usage, I/O, or assemble error (`afas:file:line: …`) |

---

## Assembly language (`.afb`)

One instruction per line. Case-insensitive mnemonics and registers. Labels are case-insensitive. Comments start with `;` and run to end of line.

```
; optional entry (default: first instruction, address 0)
.entry  start

start:
        MOVI    R1, 2
        MOVI    R2, 3
        ADD     R1, R1, R2
        OUT     1, R1
        HLT
```

### Layout

| Element | Form |
|---|---|
| Comment | `; …` |
| Label | `name:`  (own line, or before an instruction) |
| Directive | `.entry label` |
| Instruction | `MNEMONIC  operands` |

Label names: letter or `_`, then letters, digits, `_`. Each instruction is 4 bytes. A label’s address is the byte offset of the next instruction.

### Registers

`R1` … `R8` (encoded 0 … 7). No `R0`.

### Immediates

| Form | Example |
|---|---|
| Decimal | `42`, `-4` |
| Hex | `0x2A` |
| Character | `'A'`, `'\n'`, `'\t'`, `'\0'` |

### Memory operand (`LOAD` / `STORE`)

```
[R2]
[R2+8]
[R2-4]
```

Displacement is a signed 12-bit field (`-2048` … `2047`).

### Operand cheat sheet

| Mnemonic | Operands |
|---|---|
| `NOP` `HLT` `RET` | (none) |
| `INC` `DEC` `POP` | `rd` |
| `PUSH` | `rs1` |
| `MOV` `XCHG` `NEG` `NOT` | `rd, rs1` |
| `ADD` `SUB` `MUL` `DIV` `MOD` | `rd, rs1, rs2` |
| `AND` `OR` `XOR` `SHL` `SHR` `SAR` | `rd, rs1, rs2` |
| `MOVI` | `rd, imm16` (unsigned 0…65535) |
| `ADDI` `SUBI` | `rd, rs1, imm16` (signed) |
| `CMP` `TEST` | `rs1, rs2` |
| `CMPI` | `rs1, imm16` (signed) |
| `LOAD` | `rd, [rs1+disp]` |
| `STORE` | `rd, [rs1+disp]`  (`rd` is the value stored) |
| `JMP` `JE` `JNE` `JZ` `JNZ` `JL` `JLE` `JG` `JGE` `CALL` | `label` or absolute byte address |
| `IN` | `rd, port` |
| `OUT` | `port, rs1` |
| `TRAP` | `imm16` |

`OUT 0, R1` writes the low byte of `R1` to stdout. `OUT 1, R1` prints `R1` as unsigned decimal plus newline.

Jumps are **absolute** payload offsets (same as the ISA). Prefer labels; `afas` fills in the address.

---

## Samples

| Source | Assemble | Run |
|---|---|---|
| [`../examples/add.afb`](../examples/add.afb) | `afas\afas.exe examples/add.afb` | `alif.exe examples/add.afbin` → `5` |
| [`../examples/hello.afb`](../examples/hello.afb) | `afas\afas.exe examples/hello.afb` | `alif.exe examples/hello.afbin` → `ALIF` |
| [`../examples/branch.afb`](../examples/branch.afb) | `afas\afas.exe examples/branch.afb` | `alif.exe examples/branch.afbin` → `1` |
