# ALIF Instruction Set Architecture

**Status:** numeric contract in `include/opcodes.h`. Execution engine in `src/vm.c`. `.afbin` loader in `src/load.c`. Host launcher binary **`alif`** in `src/alif.c`. Semantic rules here match that interpreter.

**Machine class:** 32-bit, register-register (load/store), **Harvard v1** (bytecode payload ≠ data RAM), in-order fetch-execute.

This document is the architect’s contract. If C code and this file disagree on a *number*, `opcodes.h` wins. If they disagree on *behaviour*, this file wins until the header **and** `src/vm.c` are updated in the same change.

---

## 1. Design goals

1. **Compact encoding.** One 32-bit word per instruction. No prefixes, no variable-length ops.
2. **Trivial decode.** Fixed field positions. Opcode is always bits `[31:24]`.
3. **Small GPR file.** Eight registers keep the register field to 3 useful bits (stored in a 4-bit slot for future growth).
4. **Class-nibble opcodes.** The high nibble of the opcode is the functional class, so a decoder can switch on `op >> 4` before the exact op.
5. **Host-simple C.** Two’s complement, wrapping 32-bit arithmetic, no IEEE unit in v1.

Non-goals for v1: privilege rings, virtual memory, floating point, atomics, delay slots, predication.

---

## 2. Programmer-visible state

| State | Width | Addressable? | Reset |
|---|---|---|---|
| `R1`..`R8` | 32-bit `int` each | yes, via `rd`/`rs1`/`rs2` | `0` |
| `IP` | 32 bits | no (byte index into the **bytecode payload**) | `0` (start of payload) |
| `SP` | 32 bits | no (byte address in **RAM only**) | `ALIF_RAM_SIZE` (1024) |
| `FLAGS` | 32 bits, 4 defined | no (written by arithmetic/compare, read by `Jcc`) | `0` |
| RAM | 1024 bytes | yes, via `LOAD`/`STORE`/stack | all zeros |
| Bytecode | `unsigned char[]` | execute-only; IP fetches 4 LE bytes per insn | caller-supplied |

v1 is **Harvard**: the instruction pointer never reads `ram[]`, and `LOAD`/`STORE` never read the bytecode payload. A future image loader may copy code into a larger unified map; the opcode encodings will not change.

The engine field is named `ip` (instruction pointer). Older notes called this `PC`; they are the same register.

There is **no `R0`**. The eight GPRs are named `R1`..`R8` and encoded as **0..7**. The name is 1-based for humans; the encoding is 0-based so a 3-bit field packs tightly.

```
Human name:   R1  R2  R3  R4  R5  R6  R7  R8
Encoding:      0   1   2   3   4   5   6   7
```

A 4-bit register field (`REG_MASK = 0xF`) is reserved. Encodings `8`..`15` are **illegal**. An interpreter must trap (see §8) rather than silently alias them onto `R1`..`R8`.

`IP`, `SP`, and `FLAGS` are architectural but **not** in the register field. Software cannot `MOV` into `IP`. To change control flow, use the `0x5_` class. To change `SP`, the only v1 operations are stack instructions (they adjust `SP` themselves).

Suggested software convention (not enforced by hardware):

| Register | Role |
|---|---|
| `R1` | primary return / first argument |
| `R2`..`R4` | arguments / scratch |
| `R5`..`R6` | callee-saved (software convention) |
| `R7` | second scratch |
| `R8` | frame pointer, or extra scratch if no FP |

---

## 3. Data representation

- Integers are **32-bit two’s complement**, stored in the engine as `int regs[8]`.
- All GPR arithmetic wraps mod 2³².
- **RAM** is 1024 bytes (`ALIF_RAM_SIZE`), byte-addressable, addresses `0`..`1023`. The unit of `LOAD`/`STORE`/`PUSH`/`POP` is one **little-endian 32-bit word** entirely inside that block (last legal address `1020`).
- **Bytecode** is a caller-owned `unsigned char` array. Each instruction is four little-endian bytes. `IP` is a byte offset into that array, not into RAM.
- Address math for RAM uses signed 12-bit displacement plus the unsigned base in `rs1`. The engine evaluates the sum in 64 bits so a wrap cannot land back inside the 1 KiB window.
- Bit diagrams in this document number bits the architectural way: bit 31 is the MSB of the word, bit 0 is the LSB. That numbering is independent of byte order in the payload.

Sign-extension rules for immediates:

| Field | Bits | Arithmetic ops (`ADDI`, `SUBI`, `CMPI`, `LOAD`/`STORE` disp) | Other |
|---|---|---|---|
| `imm12` | 12 | signed (`[-2048, +2047]`) | — |
| `imm16` | 16 | signed for `ADDI`/`SUBI`/`CMPI` | **zero-extended** for `MOVI`; **unsigned port** for `IN`/`OUT`; **unsigned trap code** for `TRAP` |
| `imm24` | 24 | — | **unsigned byte address** of the target instruction; must be 4-byte aligned |

`imm24` is an **absolute byte offset into the bytecode payload**, not IP-relative and not a RAM address. IP-relative jumps can be added later in unused opcode space (`0x7_`) without breaking this encoding. The payload may be larger than 1 KiB; only jumps are limited to 24-bit offsets (`0`..`0xFFFFFF`). The 1 KiB cap applies to **RAM**, not to code size.

---

## 4. Instruction formats

Every instruction is one 32-bit word. The three formats are overlays of the **same** bits. The opcode determines which overlay is used. Fields that a format does not consume must be encoded as **zero** by assemblers (reserved for future sub-ops). Interpreters should ignore reserved-zero bits in v1.

### 4.1 R-type — register × register

Used by: `MOV`, `XCHG`, `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`, `AND`, `OR`, `XOR`, `NOT`, `SHL`, `SHR`, `SAR`, `CMP`, `TEST`, `INC`, `DEC`, `PUSH`, `POP`.

```
 31          24 23      20 19      16 15      12 11               0
+--------------+----------+----------+----------+------------------+
|    opcode    |    rd    |   rs1    |   rs2    |      imm12       |
+--------------+----------+----------+----------+------------------+
     8 bits        4 bits     4 bits     4 bits        12 bits
```

`imm12` is **zero** except on `LOAD`/`STORE`, which are R-type with a displacement (sometimes called D-type; ALIF treats them as R-type with live `imm12`).

### 4.2 I-type — register × immediate

Used by: `MOVI`, `ADDI`, `SUBI`, `CMPI`, `IN`, `OUT`, `TRAP`.

```
 31          24 23      20 19      16 15                           0
+--------------+----------+----------+-----------------------------+
|    opcode    |    rd    |   rs1    |            imm16            |
+--------------+----------+----------+-----------------------------+
     8 bits        4 bits     4 bits              16 bits
```

I-type **reinterprets** bits `[15:0]` as a single immediate. There is no `rs2`.

### 4.3 J-type — control transfer

Used by: `JMP`, `Jcc`, `CALL`.

```
 31          24 23                                                 0
+--------------+---------------------------------------------------+
|    opcode    |                      imm24                        |
+--------------+---------------------------------------------------+
     8 bits                         24 bits
```

`rd`/`rs1`/`rs2` do not exist in this overlay. `RET` is J-type in class but uses **no** immediate (`imm24` must be zero); the return address comes from the stack.

### 4.4 Field extraction (all formats)

```
op   = (word >> 24) & 0xFF
rd   = (word >> 20) & 0x0F
rs1  = (word >> 16) & 0x0F
rs2  = (word >> 12) & 0x0F
imm12 =  word        & 0x0FFF
imm16 =  word        & 0xFFFF
imm24 =  word        & 0xFFFFFF
```

These match `OP_SHIFT`, `RD_SHIFT`, `RS1_SHIFT`, `RS2_SHIFT`, and the `*_MASK` macros in `opcodes.h`.

### 4.5 Word construction (assembler)

```
R:  word = (op << 24) | (rd << 20) | (rs1 << 16) | (rs2 << 12) | (imm12 & 0x0FFF)
I:  word = (op << 24) | (rd << 20) | (rs1 << 16) | (imm16 & 0xFFFF)
J:  word = (op << 24) | (imm24 & 0xFFFFFF)
```

---

## 5. Opcode map

The opcode is 8 bits (`0x00`..`0xFF`). The **high nibble is the class**. Unused values inside a class, and entire unused classes (`0x7_`, `0x9_`..`0xE_`), are reserved. Reserved opcodes must **TRAP** (illegal instruction), not NOP.

```
High nibble     Class                 Defined ops
0x0             data movement         NOP MOV MOVI LOAD STORE XCHG
0x1             arithmetic            ADD ADDI SUB SUBI MUL DIV MOD INC DEC NEG
0x2             bitwise logic         AND OR XOR NOT
0x3             shifts                SHL SHR SAR
0x4             compare               CMP CMPI TEST
0x5             control flow          JMP JZ JNZ JE JNE JL JLE JG JGE CALL RET
0x6             stack                 PUSH POP
0x7             reserved
0x8             I/O                   IN OUT
0x9..0xE        reserved
0xF             system                (0xF0..0xFD reserved) TRAP HLT
```

`JE` is an encoding alias of `JZ` (same condition). `JNE` is an alias of `JNZ`. Both encodings must be accepted. Assemblers may emit either; `JE`/`JNE` are preferred after `CMP`, `JZ`/`JNZ` after `TEST`.

---

## 6. FLAGS

`FLAGS` is a 32-bit register. Only the low 4 bits are defined. Bits `[31:4]` must be written as 0 and ignored on read.

| Bit | Macro | Set when |
|---|---|---|
| 0 | `FLAG_CF` | unsigned carry-out (add) or borrow (sub/cmp) |
| 1 | `FLAG_ZF` | result is `0` |
| 2 | `FLAG_SF` | result bit 31 is `1` |
| 3 | `FLAG_OF` | signed two’s-complement overflow |

**Which instructions write FLAGS**

| Writes FLAGS | Does not write FLAGS |
|---|---|
| `ADD` `ADDI` `SUB` `SUBI` `NEG` `INC` `DEC` | `MOV` `MOVI` `LOAD` `STORE` `XCHG` |
| `MUL` `DIV` `MOD` (ZF/SF only; CF=OF=0) | `AND` `OR` `XOR` `NOT` (v1: **do** write ZF/SF; CF=OF=0) |
| `SHL` `SHR` `SAR` (ZF/SF; CF = last bit shifted out; OF undefined for count≠1, set 0) | `JMP` `Jcc` `CALL` `RET` |
| `CMP` `CMPI` `TEST` | `PUSH` `POP` `IN` `OUT` `NOP` `TRAP` `HLT` |

`CMP`/`CMPI` compute `rs1 - rs2` (or `rs1 - imm16`) **without** writing a GPR. FLAGS are set exactly as for `SUB`.

`TEST` computes `rs1 & rs2` without writing a GPR. FLAGS: ZF/SF from the AND result, CF=0, OF=0.

**Condition codes for `Jcc`** (identical to a simplified x86):

| Op | Condition | Meaning after `CMP rs1, rs2` (`rs1 ? rs2`) |
|---|---|---|
| `JZ` / `JE` | `ZF` | equal / result zero |
| `JNZ` / `JNE` | `!ZF` | not equal |
| `JL` | `SF != OF` | signed less |
| `JLE` | `ZF \|\| (SF != OF)` | signed less or equal |
| `JG` | `!ZF && (SF == OF)` | signed greater |
| `JGE` | `SF == OF` | signed greater or equal |

Unsigned branches (`JB`/`JAE` on CF) are **not** in v1. Use `CMP` + `JL` only when both operands are signed, or add `0x5B`+ later.

---

## 7. Instruction reference

Notation:

- `R[rd]` — GPR selected by the `rd` field (must be 0..7).
- `sext12(x)` / `sext16(x)` — sign-extend to 32 bits.
- `mem32[addr]` — little-endian 32-bit word at **RAM** byte address `addr`. `addr` must be 4-byte aligned and `addr+3 < 1024`.
- After every instruction that does not write `IP`, `IP <- IP + 4`.
- Jump instructions that **do not** take the branch still fall through (`IP + 4`).
- Fetch: if `IP` is unaligned, or `IP+4` would run past `code_len`, the engine faults **before** reading. It never indexes `code[code_len]` or `ram[1024]`.

### 7.1 Data movement — `0x0_`

#### `OP_NOP` `0x00` — R-type (all fields 0)

No operation. `IP <- IP + 4`. FLAGS unchanged.

#### `OP_MOV` `0x01` — R-type

`R[rd] <- R[rs1]`

`rs2` and `imm12` must be 0. FLAGS unchanged.

#### `OP_MOVI` `0x02` — I-type

`R[rd] <- zero_extend_16(imm16)`

`rs1` must be 0. To load a full 32-bit constant, emit `MOVI` for the low 16 bits then `ADDI`/`SHL` sequences, or (later) a `MOVHI` in reserved space.

#### `OP_LOAD` `0x03` — R-type with disp

`addr <- R[rs1] + sext12(imm12)`
`R[rd] <- mem32[addr]`

`rs2` must be 0. Unaligned `addr` → alignment fault. Out-of-range `addr` → memory fault.

#### `OP_STORE` `0x04` — R-type with disp

`addr <- R[rs1] + sext12(imm12)`
`mem32[addr] <- R[rd]`

Note: the **source** of a store is `rd`, not `rs2`. This keeps the destination/source slot stable across LOAD and STORE (same `rd` field). `rs2` must be 0.

#### `OP_XCHG` `0x05` — R-type

`tmp <- R[rd]; R[rd] <- R[rs1]; R[rs1] <- tmp`

FLAGS unchanged. `rs2` must be 0.

### 7.2 Arithmetic — `0x1_`

All of these write FLAGS as specified in §6. `rd` may alias `rs1` or `rs2`.

#### `OP_ADD` `0x10` — R-type

`R[rd] <- R[rs1] + R[rs2]`  (mod 2³²)

CF = carry out of bit 31. OF = signed overflow.

#### `OP_ADDI` `0x11` — I-type

`R[rd] <- R[rs1] + sext16(imm16)`

#### `OP_SUB` `0x12` — R-type

`R[rd] <- R[rs1] - R[rs2]`

CF = borrow. OF = signed overflow.

#### `OP_SUBI` `0x13` — I-type

`R[rd] <- R[rs1] - sext16(imm16)`

#### `OP_MUL` `0x14` — R-type

`R[rd] <- (R[rs1] * R[rs2]) & 0xFFFFFFFF`

Low 32 bits of the unsigned product. ZF/SF from that result. CF=0, OF=0. (A widening multiply can occupy `0x1A` later.)

#### `OP_DIV` `0x15` — R-type

Unsigned division: `R[rd] <- R[rs1] / R[rs2]`.

If `R[rs2] == 0` → divide-by-zero fault. ZF/SF from the quotient. CF=0, OF=0.

#### `OP_MOD` `0x16` — R-type

Unsigned remainder: `R[rd] <- R[rs1] % R[rs2]`. Divide-by-zero as `DIV`.

#### `OP_INC` `0x17` — R-type

`R[rd] <- R[rd] + 1`

Reads and writes `rd` only. `rs1`/`rs2` must be 0. FLAGS as `ADD` with 1.

#### `OP_DEC` `0x18` — R-type

`R[rd] <- R[rd] - 1`. FLAGS as `SUB` with 1.

#### `OP_NEG` `0x19` — R-type

`R[rd] <- 0 - R[rs1]`. FLAGS as `SUB`. `rs2` must be 0.

### 7.3 Bitwise logic — `0x2_`

#### `OP_AND` `0x20` / `OP_OR` `0x21` / `OP_XOR` `0x22` — R-type

`R[rd] <- R[rs1] {&\|^} R[rs2]`

ZF/SF from result. CF=0, OF=0.

#### `OP_NOT` `0x23` — R-type

`R[rd] <- ~R[rs1]`. `rs2` must be 0. FLAGS as above.

### 7.4 Shifts — `0x3_`

Shift count is `R[rs2] & 31` (only the low 5 bits). Count `0` is a legal no-op on the value; FLAGS still update from the original value; CF is 0.

#### `OP_SHL` `0x30` — logical left

`R[rd] <- R[rs1] << count`

CF = last bit shifted out of bit 31 (0 if count is 0).

#### `OP_SHR` `0x31` — logical right

`R[rd] <- R[rs1] >> count`  (zero-fill)

CF = last bit shifted out of bit 0.

#### `OP_SAR` `0x32` — arithmetic right

`R[rd] <- (int32_t)R[rs1] >> count`  (sign-fill)

CF = last bit shifted out of bit 0.

### 7.5 Compare — `0x4_`

No GPR write.

#### `OP_CMP` `0x40` — R-type

FLAGS from `R[rs1] - R[rs2]`. `rd` must be 0.

#### `OP_CMPI` `0x41` — I-type

FLAGS from `R[rs1] - sext16(imm16)`. `rd` must be 0.

#### `OP_TEST` `0x42` — R-type

FLAGS from `R[rs1] & R[rs2]`. `rd` must be 0.

### 7.6 Control flow — `0x5_`

All targets are **absolute byte offsets into the bytecode payload**. `imm24` is the value written to `IP` when the branch is taken. Payload size is not capped at 16 MiB, but a J-type can only name the first 16 MiB of the payload. RAM stays 1 KiB regardless of payload length.

Target must be 4-byte aligned **and** `target + 4 <= code_len`. Unaligned or out-of-payload targets fault. The alignment/range check runs only when the branch is taken (or on `JMP`/`CALL`/`RET`, which always take).

#### `OP_JMP` `0x50` — J-type

`IP <- imm24`

#### `OP_JZ` `0x51` / `OP_JE` `0x53`

If `ZF`: `IP <- imm24`, else `IP <- IP + 4`.

#### `OP_JNZ` `0x52` / `OP_JNE` `0x54`

If `!ZF`: `IP <- imm24`, else fall through.

#### `OP_JL` `0x55` / `OP_JLE` `0x56` / `OP_JG` `0x57` / `OP_JGE` `0x58`

Conditions in §6.

#### `OP_CALL` `0x59` — J-type

```
SP <- SP - 4
ram32[SP] <- IP + 4          # return address (payload offset)
IP <- imm24
```

If `SP` would become unaligned or leave `0..1023`, → stack fault. `CALL` does not write FLAGS.

#### `OP_RET` `0x5A` — J-type, `imm24 = 0`

```
IP <- ram32[SP]
SP <- SP + 4
```

The popped value is checked as a jump target (`jump_ok`) before it is written to `IP`.

### 7.7 Stack — `0x6_`

The stack lives in **RAM**, grows **down** (toward lower addresses). After `alif_vm_init`, `SP = 1024` (empty: one past the last RAM byte). After `PUSH`, `SP` points at the last occupied word.

```
PUSH:  SP <- SP - 4;  ram32[SP] <- R[rs1]
POP:   R[rd] <- ram32[SP];  SP <- SP + 4
```

`PUSH` uses `rs1` (source). `POP` uses `rd` (destination). Unused register fields must be 0. FLAGS unchanged.

Empty-stack `POP` or overflow `PUSH` → stack fault.

### 7.8 I/O — `0x8_`

Ports are a separate 16-bit namespace, **not** memory-mapped in v1. The interpreter binds well-known ports. Unbound ports → I/O fault.

| Port (`imm16`) | Direction | Meaning (v1 host binding) |
|---|---|---|
| `0` | `OUT` | write low 8 bits of `R[rs1]` to host stdout |
| `0` | `IN` | read one byte from host stdin into `R[rd]` (upper bits 0); `-1` (0xFFFFFFFF) on EOF |
| `1` | `OUT` | write `R[rs1]` as a decimal unsigned integer plus newline |
| `2` | `OUT` | write `R[rs1]` as 8 hex digits plus newline |
| other | — | reserved; fault until defined |

#### `OP_IN` `0x80` — I-type

`R[rd] <- port[imm16]`

`rs1` must be 0.

#### `OP_OUT` `0x81` — I-type

`port[imm16] <- R[rs1]`

`rd` must be 0. FLAGS unchanged.

### 7.9 System — `0xF_`

#### `OP_TRAP` `0xFE` — I-type

Software interrupt. `imm16` is the trap number. `rd`/`rs1` must be 0.

v1 interpreter behaviour: treat as a **controlled halt** that reports `imm16` to the host (useful for tests: `TRAP 0` = success, `TRAP 1` = failure). A later kernel can vector this.

#### `OP_HLT` `0xFF` — R-type, all fields 0

Stop fetch-execute. `IP` is left at the `HLT` instruction (not advanced). `alif_exec` returns `ALIF_OK`. Same freeze-on-instruction behaviour for `TRAP`.

---

## 8. Faults

A fault stops the machine. `alif_exec` returns the fault code; `vm->ip` is the faulting instruction (the word just fetched). `vm->fault` holds the same code.

| Code | Macro | Cause |
|---|---|---|
| 0 | `ALIF_OK` | `HLT` or `TRAP` (not a fault) |
| 1 | `ALIF_FAULT_ILL` | unknown opcode, register id 8..15, `vm == NULL` |
| 2 | `ALIF_FAULT_ALIGN` | `IP` or RAM address not multiple of 4 |
| 3 | `ALIF_FAULT_MEM` | `IP` fetch past `code_len`, RAM address outside `0..1020`, NULL/short payload, `IP+4` wrap |
| 4 | `ALIF_FAULT_DIV0` | `DIV`/`MOD` with `rs2 == 0` |
| 5 | `ALIF_FAULT_STK` | stack overflow / underflow (`SP` would leave RAM) |
| 6 | `ALIF_FAULT_IO` | unbound I/O port |
| 7 | `ALIF_FAULT_STEP` | more than `ALIF_MAX_STEPS` (1 000 000) instructions — infinite-loop guard |

Faults do not run user-mode handlers in v1. The engine returns immediately; it does not keep fetching.

---

## 9. Worked encodings

Values below are the 32-bit architectural word, written as `0xAABBCCDD` (MSB first). On disk in little-endian they appear as bytes `DD CC BB AA`.

### `MOV R1, R2` — copy R2 into R1

```
op=0x01  rd=R1=0  rs1=R2=1  rs2=0  imm12=0
word = 0x01 << 24 | 0 << 20 | 1 << 16 = 0x01010000
```

### `ADDI R3, R1, 42`

```
op=0x11  rd=R3=2  rs1=R1=0  imm16=42
word = 0x1120002A
```

### `ADD R1, R1, R2`

```
op=0x10  rd=0  rs1=0  rs2=1
word = 0x10001000
```

### `LOAD R4, [R5+8]`

```
op=0x03  rd=R4=3  rs1=R5=4  rs2=0  imm12=8
word = 0x03408008
```

### `STORE R4, [R5-4]`

```
imm12 = -4 = 0xFFC (12-bit two’s complement)
op=0x04  rd=R4=3  rs1=R5=4
word = 0x04308FFC
```

### `JMP 0x00001000`

```
op=0x50  imm24=0x001000
word = 0x50001000
```

### `OUT 1, R1` — print R1 as decimal

```
op=0x81  rd=0  rs1=R1=0  imm16=1
word = 0x81000001
```

### `HLT`

```
word = 0xFF000000
```

### Tiny program: `R1 = 2 + 3; print; halt`

```
MOVI R1, 2        0x02000002
MOVI R2, 3        0x02100003
ADD  R1, R1, R2   0x10001000
OUT  1, R1        0x81000001
HLT               0xFF000000
```

---

## 10. How a program is presented

### 10.1 In-memory API

Callers may still pass a raw bytecode payload to the engine:

```c
struct alif_vm vm;
unsigned char payload[] = { /* LE instruction words */ };
alif_vm_init(&vm);
alif_exec(&vm, payload, sizeof payload);           /* IP = 0 */
alif_exec_from(&vm, payload, sizeof payload, entry); /* IP = entry */
```

| Region | Size | Pointer | Notes |
|---|---|---|---|
| Bytecode | `code_len` bytes | caller’s `const unsigned char *` | read-only; `IP` indexes here |
| RAM | 1024 bytes | `vm.ram[]` | data + stack; `SP` starts at 1024 |
| Registers | 8 × `int` | `vm.regs[]` | `regs[R1]` is the first GPR |

### 10.2 On-disk `.afbin` image (launcher `alif`)

A complete ALIF **binary** program is a `.afbin` file. `alif.exe` / `alif` loads that file and runs the VM. The launcher does **not** read assembly.

Optional path: write `.afb` assembly and run the separate assembler **`afas`** (`afas/`) to emit `.afbin`. That tool is not part of the VM; skipping it does not change `alif` behaviour.

Worked images: `examples/add.afbin`, `examples/hello.afbin`. Assembly sources for the same programs: `examples/add.afb`, `examples/hello.afb` (see [`afas/README.md`](../afas/README.md)).

The `alif` binary (`src/alif.c` + `src/load.c`) reads a **version-1.0** image. Extension is `.afbin` (required by the launcher, any case). Magic is the four ASCII bytes `A L I F` (not NUL-terminated). All multi-byte header fields are **little-endian**. File length must equal `32 + code_size + data_size` exactly (no padding, no trailing bytes).

```
Offset   Size     Field
0x00     4        magic  'A' 'L' 'I' 'F'
0x04     2        version major = 1
0x06     2        version minor = 0
0x08     4        entry IP (byte offset into the code section, 4-aligned)
0x0C     4        code size in bytes (multiple of 4, at least 4, ≤ 16 MiB)
0x10     4        data size in bytes (multiple of 4, ≤ 1024)
0x14     4        reserved, must be 0
0x18     8        reserved, must be 0
0x20     code     instruction stream → `alif_exec_from` payload
+code    data     copied to `vm.ram[0 .. data_size)` before run
```

Loader steps:

1. Require the path to end in `.afbin` (any case).
2. Open the file (`rb`), reject anything smaller than 32 bytes.
3. Check magic, version `1.0`, reserved zeros, sizes, and entry.
4. `malloc` the code section; copy data into `struct alif_image`.
5. `alif_vm_init`; `memcpy` data into `vm.ram` if `data_size > 0`.
6. `alif_exec_from(vm, code, code_size, entry)`.
7. Free the code buffer.

`code_size` is **not** capped at 1 KiB. The 1 KiB cap is RAM only. J-type `imm24` can still only name the first 16 MiB of the payload, which is also the loader’s max `code_size`.

```
alif program.afbin
```

| `alif` exit | Meaning |
|---|---|
| 0 | `HLT`, or `TRAP` with imm16 0 |
| 1 | usage, I/O, or format error (message on stderr) |
| 2 | VM fault (`ILL`/`ALIGN`/`MEM`/…); `ip` printed on stderr |
| 1–255 | `TRAP` imm16 when nonzero (host exit status) |

### 10.3 Assembly source (`.afb`) — tool `afas`

Not consumed by the VM. The assembler lives in `afas/` and is optional:

```
afas program.afb              # writes program.afbin
alif program.afbin            # unchanged launcher
```

Language contract: [`afas/README.md`](../afas/README.md). Adding an opcode requires a mnemonic in `afas/afas.c` **in addition to** `opcodes.h` / `vm.c`; `alif` does not need `afas` to keep running existing `.afbin` files.

---

## 11. What is deliberately absent

| Missing | Why | Likely home later |
|---|---|---|
| `R0` wired to zero | 8 named GPRs were required; a zero reg would be a 9th id | use `MOVI rd, 0` |
| IP-relative branches | keeps J-type decode a pure immediate | `0x7_` class |
| Register-indirect jump | 24-bit abs offsets cover small payloads | `OP_JMPR` on `0x5B` |
| Unified code/data map | 1 KiB RAM + separate payload is simpler to bound | optional later |
| `MOVHI` / 32-bit load immediate | 16-bit `MOVI` is enough to bootstrap | `0x06` |
| Unsigned `Jcc` on CF | signed set is enough for first compiler | `0x5C`..`0x5F` |
| Byte/halfword load-store | word machine first | `0x07`..`0x0A` |
| Privilege / syscall ABI | single-process interpreter | expand `TRAP` |

---

## 12. Source of truth

| Item | Lives in |
|---|---|
| Opcode numbers, register ids, field shifts, flag bits | `include/opcodes.h` |
| Machine struct, RAM size, fault macros, `alif_exec` | `include/alif.h` |
| `.afbin` header, `alif_load_afbin` / `alif_write_afbin` | `include/alf.h`, `src/load.c` |
| Launcher CLI | `src/alif.c` (binary: `alif`) |
| Optional assembler `.afb` → `.afbin` | `afas/afas.c` (binary: `afas`) |
| 8-bit Urdu source encoding (`.alif`, not VM) | `af8/` |
| علیف language grammar | `lang/` |
| Compiler `.alif` → `.afb` | `alifc/alifc.c` (binary: `alifc`) |
| Fetch-decode-execute loop and bounds checks | `src/vm.c` |
| Behaviour, Harvard split, I/O ports, encoding examples | this file |
| How the C loop is structured | `docs/IMPLEMENTATION.md` |

When adding an opcode: assign the number in `opcodes.h`, implement a `case` in `src/vm.c`, add a row to §5 and a subsection to §7 **in the same change**. If the assembler should accept it, add a mnemonic in `afas/afas.c` (optional; does not affect `alif`).
