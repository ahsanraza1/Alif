#ifndef ALIF_OPCODES_H
#define ALIF_OPCODES_H

/*
 * ALIF — 32-bit register VM  (ISA numeric contract)
 *
 * Canonical documentation:
 *   docs/ISA.md              instruction set, encodings, semantics
 *   docs/IMPLEMENTATION.md   decoder, memory, flags, binary image
 *   README.md                project index
 *
 * Instruction word (MSB = bit 31). Three overlays of the same 32 bits:
 *
 * R-type  [31:24] op | [23:20] rd | [19:16] rs1 | [15:12] rs2 | [11:0] imm12
 * I-type  [31:24] op | [23:20] rd | [19:16] rs1 | [15:0]  imm16
 * J-type  [31:24] op | [23:0]  imm24
 *
 * Register field is 4 bits; only ids 0..7 (R1..R8) are valid.
 */

#define ALIF_WORD_BITS          32
#define ALIF_INSN_BYTES         4
#define ALIF_NREGS              8

/* ---- field positions ------------------------------------------------ */
#define OP_SHIFT                24
#define OP_MASK                 0xFFu
#define RD_SHIFT                20
#define RS1_SHIFT               16
#define RS2_SHIFT               12
#define REG_MASK                0x0Fu
#define IMM12_MASK              0x0FFFu
#define IMM16_MASK              0xFFFFu
#define IMM24_MASK              0xFFFFFFu

/* ---- general-purpose registers (3-bit ids, R1..R8) ------------------ */
#define R1                      0x0
#define R2                      0x1
#define R3                      0x2
#define R4                      0x3
#define R5                      0x4
#define R6                      0x5
#define R7                      0x6
#define R8                      0x7

/* ---- FLAGS bits (architectural, not a GPR) -------------------------- */
#define FLAG_CF                 (1u << 0)   /* carry / unsigned overflow */
#define FLAG_ZF                 (1u << 1)   /* zero                      */
#define FLAG_SF                 (1u << 2)   /* sign  (bit 31 of result)  */
#define FLAG_OF                 (1u << 3)   /* signed overflow           */

/* ---- opcode map (high nibble = class) ------------------------------- */

/* 0x0_  data movement */
#define OP_NOP                  0x00
#define OP_MOV                  0x01    /* rd = rs1                     */
#define OP_MOVI                 0x02    /* rd = imm16                   */
#define OP_LOAD                 0x03    /* rd = mem[rs1 + imm12]        */
#define OP_STORE                0x04    /* mem[rs1 + imm12] = rd        */
#define OP_XCHG                 0x05    /* rd <-> rs1                   */

/* 0x1_  arithmetic */
#define OP_ADD                  0x10    /* rd = rs1 + rs2               */
#define OP_ADDI                 0x11    /* rd = rs1 + imm16             */
#define OP_SUB                  0x12    /* rd = rs1 - rs2               */
#define OP_SUBI                 0x13    /* rd = rs1 - imm16             */
#define OP_MUL                  0x14    /* rd = rs1 * rs2               */
#define OP_DIV                  0x15    /* rd = rs1 / rs2               */
#define OP_MOD                  0x16    /* rd = rs1 % rs2               */
#define OP_INC                  0x17    /* rd = rd + 1                  */
#define OP_DEC                  0x18    /* rd = rd - 1                  */
#define OP_NEG                  0x19    /* rd = -rs1                    */

/* 0x2_  bitwise logic */
#define OP_AND                  0x20    /* rd = rs1 & rs2               */
#define OP_OR                   0x21    /* rd = rs1 | rs2               */
#define OP_XOR                  0x22    /* rd = rs1 ^ rs2               */
#define OP_NOT                  0x23    /* rd = ~rs1                    */

/* 0x3_  shifts */
#define OP_SHL                  0x30    /* rd = rs1 << rs2              */
#define OP_SHR                  0x31    /* rd = rs1 >> rs2  (logical)   */
#define OP_SAR                  0x32    /* rd = rs1 >> rs2  (arith)     */

/* 0x4_  compare (sets FLAGS, no dest write) */
#define OP_CMP                  0x40    /* flags = rs1 - rs2            */
#define OP_CMPI                 0x41    /* flags = rs1 - imm16          */
#define OP_TEST                 0x42    /* flags = rs1 & rs2            */

/* 0x5_  control flow */
#define OP_JMP                  0x50    /* pc = imm24                   */
#define OP_JZ                   0x51    /* if ZF  pc = imm24            */
#define OP_JNZ                  0x52    /* if !ZF pc = imm24            */
#define OP_JE                   0x53    /* if ZF  pc = imm24            */
#define OP_JNE                  0x54    /* if !ZF pc = imm24            */
#define OP_JL                   0x55    /* if SF!=OF pc = imm24         */
#define OP_JLE                  0x56    /* if ZF | (SF!=OF)             */
#define OP_JG                   0x57    /* if !ZF & (SF==OF)            */
#define OP_JGE                  0x58    /* if SF==OF                    */
#define OP_CALL                 0x59    /* push pc; pc = imm24          */
#define OP_RET                  0x5A    /* pc = pop()                   */

/* 0x6_  stack */
#define OP_PUSH                 0x60    /* mem[--sp] = rs1              */
#define OP_POP                  0x61    /* rd = mem[sp++]               */

/* 0x8_  I/O */
#define OP_IN                   0x80    /* rd = port[imm16]             */
#define OP_OUT                  0x81    /* port[imm16] = rs1            */

/* 0xF_  system */
#define OP_TRAP                 0xFE    /* software interrupt, imm16    */
#define OP_HLT                  0xFF    /* halt                         */

#endif /* ALIF_OPCODES_H */
