# ALIF programs (`.afbin`)

This directory holds programs. **`alif.exe` only runs `.afbin`.** Optional `.afb` assembly is translated by **`afas`**, a separate tool (`afas/`). The VM does not depend on `afas`.

```
programmer
    |
    |  (optional) write assembly
    v
something.afb
    |
    |  afas          ← not part of alif.exe
    v
something.afbin      ← this is what alif.exe runs
    |
    |  alif.exe
    v
VM
```

| File | What it does | Assemble | Run |
|---|---|---|---|
| [`add.afb`](add.afb) / [`add.afbin`](add.afbin) | `R1 = 2 + 6`, print `8` | `afas\afas.exe examples/add.afb` | `alif.exe examples/add.afbin` |
| [`hello.afb`](hello.afb) / [`hello.afbin`](hello.afbin) | write `ALIF` | `afas\afas.exe examples/hello.afb` | `alif.exe examples/hello.afbin` |
| [`branch.afb`](branch.afb) | `CMP` / `JE` with a label | `afas\afas.exe examples/branch.afb` | `alif.exe examples/branch.afbin` |

Assembly language: [`../afas/README.md`](../afas/README.md). The launcher **requires** the `.afbin` suffix.

You can still emit `.afbin` without `afas` (hex editor, `alif_write_afbin`, or `gcc` + `tests/mk_examples.c`).

---

## File layout (version 1.0)

Little-endian. Length on disk is exactly `32 + code_size + data_size`.

```
Offset   Size   Field
0x00     4      magic     41 4C 49 46   ASCII "ALIF"
0x04     2      ver_major 01 00         1
0x06     2      ver_minor 00 00         0
0x08     4      entry     IP into the code section (multiple of 4)
0x0C     4      code_size bytes of instructions (multiple of 4, ≥ 4)
0x10     4      data_size bytes copied to RAM[0] (multiple of 4, ≤ 1024)
0x14     4      reserved  00 00 00 00
0x18     8      reserved  zeros
0x20     code_size   instruction words (each 4 bytes, little-endian)
+code    data_size   optional RAM image
```

Each instruction word: opcode in bits `[31:24]`, then `rd`, `rs1`, `rs2`/`imm` — see `docs/ISA.md` and `include/opcodes.h`.

---

## `add.afbin` byte map (52 bytes)

Header (32 bytes):

```
41 4C 49 46   01 00 00 00   00 00 00 00   14 00 00 00
00 00 00 00   00 00 00 00   00 00 00 00   00 00 00 00
```

(`code_size = 0x14 = 20`, `entry = 0`, no data)

Code (20 bytes, five words):

```
offset  bytes         word         asm
0x20    02 00 00 02   0x02000002   MOVI R1, 2
0x24    03 00 10 02   0x02100003   MOVI R2, 3
0x28    00 10 00 10   0x10001000   ADD  R1, R1, R2
0x2C    01 00 00 81   0x81000001   OUT  1, R1      ; decimal + newline
0x30    00 00 00 FF   0xFF000000   HLT
```

---

## `hello.afbin` (print `ALIF`)

Same header pattern, `code_size = 44` (`2C 00 00 00` at offset 0x0C). Code is `MOVI R1, ch` / `OUT 0, R1` for `A L I F \n`, then `HLT`. Port 0 writes one byte to stdout.

---

## Writing your own

Until a compiler exists, write `.afb` and run `afas`, or pack words with `alif_write_afbin` (`include/alf.h`) or a hex editor. `alif` only cares about the `.afbin` rules:

- filename ends with `.afbin`
- magic must be `ALIF`
- version must be `1.0`
- reserved fields must be zero
- file size must match the header
- `entry` must be 4-aligned and inside the code section

Then:

```
alif your_program.afbin
```
