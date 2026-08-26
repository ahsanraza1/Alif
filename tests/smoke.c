#include "alif.h"

#include <stdio.h>
#include <string.h>

static void put_le(unsigned char *p, uint32_t w)
{
    p[0] = (unsigned char)(w      );
    p[1] = (unsigned char)(w >>  8);
    p[2] = (unsigned char)(w >> 16);
    p[3] = (unsigned char)(w >> 24);
}

static int fail(const char *what, int rc, const struct alif_vm *vm)
{
    fprintf(stderr, "FAIL %s rc=%d fault=%d R1=%d ip=%u\n",
            what, rc, vm->fault, vm->regs[R1], vm->ip);
    return 1;
}

int main(void)
{
    struct alif_vm vm;
    unsigned char code[32];
    int rc;

    /* 2 + 3, print, halt */
    memset(code, 0, sizeof code);
    put_le(code +  0, 0x02000002u); /* MOVI R1, 2 */
    put_le(code +  4, 0x02100003u); /* MOVI R2, 3 */
    put_le(code +  8, 0x10001000u); /* ADD  R1, R1, R2 */
    put_le(code + 12, 0x81000001u); /* OUT  1, R1 */
    put_le(code + 16, 0xFF000000u); /* HLT */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 20);
    if (rc != ALIF_OK || vm.regs[R1] != 5)
        return fail("add", rc, &vm);

    /* empty POP -> stack fault */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x61000000u); /* POP R1 */
    put_le(code + 4, 0xFF000000u);
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 8);
    if (rc != ALIF_FAULT_STK)
        return fail("pop-empty", rc, &vm);

    /* DIV by zero */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x02000001u); /* MOVI R1, 1 */
    put_le(code + 4, 0x02100000u); /* MOVI R2, 0 */
    put_le(code + 8, 0x15001000u); /* DIV  R1, R1, R2 */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 12);
    if (rc != ALIF_FAULT_DIV0)
        return fail("div0", rc, &vm);

    /* LOAD from address 1 -> align fault */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x02000001u); /* MOVI R1, 1 */
    put_le(code + 4, 0x03100000u); /* LOAD R2, [R1+0] */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 8);
    if (rc != ALIF_FAULT_ALIGN)
        return fail("align", rc, &vm);

    /* LOAD past 1 KiB RAM */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x02000400u); /* MOVI R1, 1024 */
    put_le(code + 4, 0x03100000u); /* LOAD R2, [R1+0] */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 8);
    if (rc != ALIF_FAULT_MEM)
        return fail("ram-oob", rc, &vm);

    /* illegal opcode */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x70000000u);
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 4);
    if (rc != ALIF_FAULT_ILL)
        return fail("ill-op", rc, &vm);

    /* rd = 8 is illegal */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x01800000u); /* MOV rd=8, rs1=0 */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 4);
    if (rc != ALIF_FAULT_ILL)
        return fail("ill-reg", rc, &vm);

    /* truncated payload */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0xFF000000u);
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 3);
    if (rc != ALIF_FAULT_MEM)
        return fail("trunc", rc, &vm);

    /* JMP past payload */
    memset(code, 0, sizeof code);
    put_le(code + 0, 0x50000100u); /* JMP 0x100 */
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 4);
    if (rc != ALIF_FAULT_MEM)
        return fail("jmp-oob", rc, &vm);

    /* STORE / LOAD round-trip through RAM */
    memset(code, 0, sizeof code);
    put_le(code +  0, 0x0200002Au); /* MOVI R1, 42 */
    put_le(code +  4, 0x02100000u); /* MOVI R2, 0  */
    put_le(code +  8, 0x04010000u); /* STORE R1, [R2+0] */
    put_le(code + 12, 0x03310000u); /* LOAD  R4, [R2+0] */
    put_le(code + 16, 0xFF000000u);
    alif_vm_init(&vm);
    rc = alif_exec(&vm, code, 20);
    if (rc != ALIF_OK || vm.regs[R4] != 42)
        return fail("ram-rw", rc, &vm);

    fputs("ok\n", stderr);
    return 0;
}
