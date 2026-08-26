#include "alif.h"

#include <stdio.h>
#include <string.h>

/*
 * ALIF fetch-decode-execute loop.
 *
 * Instruction bytes come from the caller’s bytecode payload (Harvard:
 * code is not in ram[]). LOAD/STORE/PUSH/POP/CALL/RET use ram[0..1023].
 * Every index is checked before a read or write.
 */

static int fault(struct alif_vm *vm, int code)
{
    vm->halt  = 1;
    vm->fault = code;
    return code;
}

static uint32_t sext12(uint32_t x)
{
    return (uint32_t)((int32_t)(x << 20) >> 20);
}

static uint32_t sext16(uint32_t x)
{
    return (uint32_t)((int32_t)(x << 16) >> 16);
}

static int reg_ok(unsigned r)
{
    return r < (unsigned)ALIF_NREGS;
}

static void flags_zsf(struct alif_vm *vm, uint32_t res)
{
    vm->flags &= ~(FLAG_ZF | FLAG_SF);
    if (res == 0u)
        vm->flags |= FLAG_ZF;
    if (res & 0x80000000u)
        vm->flags |= FLAG_SF;
}

static void flags_logic(struct alif_vm *vm, uint32_t res)
{
    vm->flags &= ~(FLAG_CF | FLAG_OF);
    flags_zsf(vm, res);
}

static void flags_add(struct alif_vm *vm, uint32_t a, uint32_t b, uint32_t res)
{
    vm->flags = 0;
    if (res < a)
        vm->flags |= FLAG_CF;
    if ((~(a ^ b) & (a ^ res)) & 0x80000000u)
        vm->flags |= FLAG_OF;
    flags_zsf(vm, res);
}

static void flags_sub(struct alif_vm *vm, uint32_t a, uint32_t b, uint32_t res)
{
    vm->flags = 0;
    if (a < b)
        vm->flags |= FLAG_CF;
    if (((a ^ b) & (a ^ res)) & 0x80000000u)
        vm->flags |= FLAG_OF;
    flags_zsf(vm, res);
}

/* Four LE bytes at ip -> 32-bit word. Fails closed on any bound error. */
static int fetch_insn(struct alif_vm *vm,
                      const unsigned char *code,
                      size_t code_len,
                      uint32_t *out)
{
    unsigned int ip = vm->ip;

    if (code == NULL)
        return fault(vm, ALIF_FAULT_MEM);
    if ((ip & 3u) != 0u)
        return fault(vm, ALIF_FAULT_ALIGN);
    if ((size_t)ip >= code_len || code_len - (size_t)ip < (size_t)ALIF_INSN_BYTES)
        return fault(vm, ALIF_FAULT_MEM);

    *out =  (uint32_t)code[ip]
         | ((uint32_t)code[ip + 1] <<  8)
         | ((uint32_t)code[ip + 2] << 16)
         | ((uint32_t)code[ip + 3] << 24);
    return ALIF_OK;
}

static int ram_addr_ok(struct alif_vm *vm, int64_t ea)
{
    if (ea < 0 || ea > (int64_t)(ALIF_RAM_SIZE - ALIF_INSN_BYTES))
        return fault(vm, ALIF_FAULT_MEM);
    if ((ea & 3) != 0)
        return fault(vm, ALIF_FAULT_ALIGN);
    return ALIF_OK;
}

static int ram_load(struct alif_vm *vm, unsigned int addr, uint32_t *out)
{
    if (ram_addr_ok(vm, (int64_t)addr) != ALIF_OK)
        return vm->fault;
    *out =  (uint32_t)vm->ram[addr]
         | ((uint32_t)vm->ram[addr + 1] <<  8)
         | ((uint32_t)vm->ram[addr + 2] << 16)
         | ((uint32_t)vm->ram[addr + 3] << 24);
    return ALIF_OK;
}

static int ram_store(struct alif_vm *vm, unsigned int addr, uint32_t val)
{
    if (ram_addr_ok(vm, (int64_t)addr) != ALIF_OK)
        return vm->fault;
    vm->ram[addr]     = (unsigned char)(val      );
    vm->ram[addr + 1] = (unsigned char)(val >>  8);
    vm->ram[addr + 2] = (unsigned char)(val >> 16);
    vm->ram[addr + 3] = (unsigned char)(val >> 24);
    return ALIF_OK;
}

/* Effective address = unsigned base + signed disp, no wrap into RAM. */
static int ea_ram(struct alif_vm *vm, unsigned rs1, uint32_t imm12, unsigned int *addr)
{
    int64_t ea;
    uint32_t base;
    int32_t  disp;

    if (!reg_ok(rs1))
        return fault(vm, ALIF_FAULT_ILL);
    base = (uint32_t)vm->regs[rs1];
    disp = (int32_t)sext12(imm12);
    ea   = (int64_t)base + (int64_t)disp;
    if (ram_addr_ok(vm, ea) != ALIF_OK)
        return vm->fault;
    *addr = (unsigned int)ea;
    return ALIF_OK;
}

static int jump_ok(struct alif_vm *vm, uint32_t target, size_t code_len)
{
    if ((target & 3u) != 0u)
        return fault(vm, ALIF_FAULT_ALIGN);
    if ((size_t)target >= code_len || code_len - (size_t)target < (size_t)ALIF_INSN_BYTES)
        return fault(vm, ALIF_FAULT_MEM);
    return ALIF_OK;
}

static int stack_push(struct alif_vm *vm, uint32_t val)
{
    if (vm->sp < (unsigned int)ALIF_INSN_BYTES || vm->sp > (unsigned int)ALIF_RAM_SIZE)
        return fault(vm, ALIF_FAULT_STK);
    vm->sp -= (unsigned int)ALIF_INSN_BYTES;
    return ram_store(vm, vm->sp, val);
}

static int stack_pop(struct alif_vm *vm, uint32_t *out)
{
    if (vm->sp > (unsigned int)(ALIF_RAM_SIZE - ALIF_INSN_BYTES))
        return fault(vm, ALIF_FAULT_STK);
    if (ram_load(vm, vm->sp, out) != ALIF_OK)
        return vm->fault;
    {
        unsigned int next = vm->sp + (unsigned int)ALIF_INSN_BYTES;
        if (next < vm->sp || next > (unsigned int)ALIF_RAM_SIZE)
            return fault(vm, ALIF_FAULT_STK);
        vm->sp = next;
    }
    return ALIF_OK;
}

void alif_vm_init(struct alif_vm *vm)
{
    if (vm == NULL)
        return;
    memset(vm, 0, sizeof(*vm));
    vm->sp = (unsigned int)ALIF_RAM_SIZE;
}

int alif_exec(struct alif_vm *vm, const unsigned char *code, size_t code_len)
{
    return alif_exec_from(vm, code, code_len, 0);
}

int alif_exec_from(struct alif_vm *vm, const unsigned char *code, size_t code_len,
                   unsigned int entry)
{
    if (vm == NULL)
        return ALIF_FAULT_ILL;

    vm->halt      = 0;
    vm->fault     = ALIF_OK;
    vm->trap_code = 0;
    vm->steps     = 0;
    vm->ip        = entry;

    if (code == NULL || code_len < (size_t)ALIF_INSN_BYTES)
        return fault(vm, ALIF_FAULT_MEM);
    if ((entry & 3u) != 0u)
        return fault(vm, ALIF_FAULT_ALIGN);
    if ((size_t)entry >= code_len || code_len - (size_t)entry < (size_t)ALIF_INSN_BYTES)
        return fault(vm, ALIF_FAULT_MEM);

    while (!vm->halt) {
        uint32_t insn;
        unsigned op, rd, rs1, rs2;
        uint32_t imm12, imm16, imm24;
        uint32_t ua, ub, ures, count, tmpu;
        unsigned int addr;
        int branched;
        int take;
        int zf, sf, of;
        int c;

        if (vm->steps >= ALIF_MAX_STEPS)
            return fault(vm, ALIF_FAULT_STEP);
        vm->steps++;

        if (fetch_insn(vm, code, code_len, &insn) != ALIF_OK)
            return vm->fault;

        op    = (unsigned)((insn >> OP_SHIFT)  & OP_MASK);
        rd    = (unsigned)((insn >> RD_SHIFT)  & REG_MASK);
        rs1   = (unsigned)((insn >> RS1_SHIFT) & REG_MASK);
        rs2   = (unsigned)((insn >> RS2_SHIFT) & REG_MASK);
        imm12 = insn & IMM12_MASK;
        imm16 = insn & IMM16_MASK;
        imm24 = insn & IMM24_MASK;
        branched = 0;

        switch (op) {

        case OP_NOP:
            break;

        case OP_MOV:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            vm->regs[rd] = vm->regs[rs1];
            break;

        case OP_MOVI:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            vm->regs[rd] = (int)(imm16 & 0xFFFFu);
            break;

        case OP_LOAD:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            if (ea_ram(vm, rs1, imm12, &addr) != ALIF_OK)
                return vm->fault;
            if (ram_load(vm, addr, &tmpu) != ALIF_OK)
                return vm->fault;
            vm->regs[rd] = (int)tmpu;
            break;

        case OP_STORE:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            if (ea_ram(vm, rs1, imm12, &addr) != ALIF_OK)
                return vm->fault;
            if (ram_store(vm, addr, (uint32_t)vm->regs[rd]) != ALIF_OK)
                return vm->fault;
            break;

        case OP_XCHG:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            {
                int t = vm->regs[rd];
                vm->regs[rd]  = vm->regs[rs1];
                vm->regs[rs1] = t;
            }
            break;

        case OP_ADD:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = (uint32_t)vm->regs[rs2];
            ures = ua + ub;
            flags_add(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_ADDI:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = sext16(imm16);
            ures = ua + ub;
            flags_add(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_SUB:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = (uint32_t)vm->regs[rs2];
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_SUBI:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = sext16(imm16);
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_MUL:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = (uint32_t)vm->regs[rs2];
            ures = ua * ub;
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_DIV:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua = (uint32_t)vm->regs[rs1];
            ub = (uint32_t)vm->regs[rs2];
            if (ub == 0u)
                return fault(vm, ALIF_FAULT_DIV0);
            ures = ua / ub;
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_MOD:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua = (uint32_t)vm->regs[rs1];
            ub = (uint32_t)vm->regs[rs2];
            if (ub == 0u)
                return fault(vm, ALIF_FAULT_DIV0);
            ures = ua % ub;
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_INC:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rd];
            ub   = 1u;
            ures = ua + ub;
            flags_add(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_DEC:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rd];
            ub   = 1u;
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_NEG:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = 0u;
            ub   = (uint32_t)vm->regs[rs1];
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_AND:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ures = (uint32_t)vm->regs[rs1] & (uint32_t)vm->regs[rs2];
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_OR:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ures = (uint32_t)vm->regs[rs1] | (uint32_t)vm->regs[rs2];
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_XOR:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ures = (uint32_t)vm->regs[rs1] ^ (uint32_t)vm->regs[rs2];
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_NOT:
            if (!reg_ok(rd) || !reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ures = ~(uint32_t)vm->regs[rs1];
            flags_logic(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_SHL:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua    = (uint32_t)vm->regs[rs1];
            count = (uint32_t)vm->regs[rs2] & 31u;
            if (count == 0u) {
                ures = ua;
                vm->flags &= ~FLAG_CF;
            } else {
                if (ua & (1u << (32u - count)))
                    vm->flags |= FLAG_CF;
                else
                    vm->flags &= ~FLAG_CF;
                ures = ua << count;
            }
            vm->flags &= ~FLAG_OF;
            flags_zsf(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_SHR:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua    = (uint32_t)vm->regs[rs1];
            count = (uint32_t)vm->regs[rs2] & 31u;
            if (count == 0u) {
                ures = ua;
                vm->flags &= ~FLAG_CF;
            } else {
                if (ua & (1u << (count - 1u)))
                    vm->flags |= FLAG_CF;
                else
                    vm->flags &= ~FLAG_CF;
                ures = ua >> count;
            }
            vm->flags &= ~FLAG_OF;
            flags_zsf(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_SAR:
            if (!reg_ok(rd) || !reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua    = (uint32_t)vm->regs[rs1];
            count = (uint32_t)vm->regs[rs2] & 31u;
            if (count == 0u) {
                ures = ua;
                vm->flags &= ~FLAG_CF;
            } else {
                if (ua & (1u << (count - 1u)))
                    vm->flags |= FLAG_CF;
                else
                    vm->flags &= ~FLAG_CF;
                ures = (uint32_t)((int32_t)ua >> (int)count);
            }
            vm->flags &= ~FLAG_OF;
            flags_zsf(vm, ures);
            vm->regs[rd] = (int)ures;
            break;

        case OP_CMP:
            if (!reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = (uint32_t)vm->regs[rs2];
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            break;

        case OP_CMPI:
            if (!reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ua   = (uint32_t)vm->regs[rs1];
            ub   = sext16(imm16);
            ures = ua - ub;
            flags_sub(vm, ua, ub, ures);
            break;

        case OP_TEST:
            if (!reg_ok(rs1) || !reg_ok(rs2))
                return fault(vm, ALIF_FAULT_ILL);
            ures = (uint32_t)vm->regs[rs1] & (uint32_t)vm->regs[rs2];
            flags_logic(vm, ures);
            break;

        case OP_JMP:
            if (jump_ok(vm, imm24, code_len) != ALIF_OK)
                return vm->fault;
            vm->ip   = (unsigned int)imm24;
            branched = 1;
            break;

        case OP_JZ:
        case OP_JE:
        case OP_JNZ:
        case OP_JNE:
        case OP_JL:
        case OP_JLE:
        case OP_JG:
        case OP_JGE:
            zf = (vm->flags & FLAG_ZF) != 0;
            sf = (vm->flags & FLAG_SF) != 0;
            of = (vm->flags & FLAG_OF) != 0;
            take = 0;
            if (op == OP_JZ || op == OP_JE)
                take = zf;
            else if (op == OP_JNZ || op == OP_JNE)
                take = !zf;
            else if (op == OP_JL)
                take = sf != of;
            else if (op == OP_JLE)
                take = zf || (sf != of);
            else if (op == OP_JG)
                take = !zf && (sf == of);
            else
                take = sf == of;
            if (take) {
                if (jump_ok(vm, imm24, code_len) != ALIF_OK)
                    return vm->fault;
                vm->ip   = (unsigned int)imm24;
                branched = 1;
            }
            break;

        case OP_CALL:
            if (jump_ok(vm, imm24, code_len) != ALIF_OK)
                return vm->fault;
            if (vm->ip > 0xFFFFFFFFu - (unsigned int)ALIF_INSN_BYTES)
                return fault(vm, ALIF_FAULT_MEM);
            if (stack_push(vm, (uint32_t)vm->ip + (uint32_t)ALIF_INSN_BYTES) != ALIF_OK)
                return vm->fault;
            vm->ip   = (unsigned int)imm24;
            branched = 1;
            break;

        case OP_RET:
            if (stack_pop(vm, &tmpu) != ALIF_OK)
                return vm->fault;
            if (jump_ok(vm, tmpu, code_len) != ALIF_OK)
                return vm->fault;
            vm->ip   = (unsigned int)tmpu;
            branched = 1;
            break;

        case OP_PUSH:
            if (!reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            if (stack_push(vm, (uint32_t)vm->regs[rs1]) != ALIF_OK)
                return vm->fault;
            break;

        case OP_POP:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            if (stack_pop(vm, &tmpu) != ALIF_OK)
                return vm->fault;
            vm->regs[rd] = (int)tmpu;
            break;

        case OP_IN:
            if (!reg_ok(rd))
                return fault(vm, ALIF_FAULT_ILL);
            if (imm16 != 0u)
                return fault(vm, ALIF_FAULT_IO);
            c = getchar();
            vm->regs[rd] = (c == EOF) ? (int)0xFFFFFFFFu : (int)(c & 0xFF);
            break;

        case OP_OUT:
            if (!reg_ok(rs1))
                return fault(vm, ALIF_FAULT_ILL);
            ua = (uint32_t)vm->regs[rs1];
            if (imm16 == 0u) {
                (void)putchar((int)(ua & 0xFFu));
                (void)fflush(stdout);
            } else if (imm16 == 1u) {
                (void)printf("%u\n", (unsigned)ua);
                (void)fflush(stdout);
            } else if (imm16 == 2u) {
                (void)printf("%08X\n", (unsigned)ua);
                (void)fflush(stdout);
            } else {
                return fault(vm, ALIF_FAULT_IO);
            }
            break;

        case OP_TRAP:
            vm->trap_code = (unsigned int)(imm16 & 0xFFFFu);
            vm->halt      = 1;
            branched      = 1; /* freeze IP on the TRAP word */
            break;

        case OP_HLT:
            vm->halt = 1;
            branched = 1; /* freeze IP on the HLT word */
            break;

        default:
            return fault(vm, ALIF_FAULT_ILL);
        }

        if (!vm->halt && !branched) {
            if (vm->ip > 0xFFFFFFFFu - (unsigned int)ALIF_INSN_BYTES)
                return fault(vm, ALIF_FAULT_MEM);
            vm->ip += (unsigned int)ALIF_INSN_BYTES;
        }
    }

    return vm->fault;
}
