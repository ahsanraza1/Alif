#include "alf.h"

#include <stdio.h>
#include <stdint.h>

static void put_le(unsigned char *p, uint32_t w)
{
    p[0] = (unsigned char)(w      );
    p[1] = (unsigned char)(w >>  8);
    p[2] = (unsigned char)(w >> 16);
    p[3] = (unsigned char)(w >> 24);
}

static int emit(const char *path, const unsigned char *code, size_t n)
{
    if (alif_write_afbin(path, code, n, NULL, 0, 0) != 0) {
        fprintf(stderr, "mk_examples: cannot write %s\n", path);
        return 1;
    }
    return 0;
}

int main(void)
{
    unsigned char add[20];
    unsigned char hello[44];

    put_le(add +  0, 0x02000002u); /* MOVI R1, 2 */
    put_le(add +  4, 0x02100003u); /* MOVI R2, 3 */
    put_le(add +  8, 0x10001000u); /* ADD  R1, R1, R2 */
    put_le(add + 12, 0x81000001u); /* OUT  1, R1 */
    put_le(add + 16, 0xFF000000u); /* HLT */

    put_le(hello +  0, 0x02000041u); /* MOVI R1, 'A' */
    put_le(hello +  4, 0x81000000u); /* OUT  0, R1    */
    put_le(hello +  8, 0x0200004Cu); /* MOVI R1, 'L' */
    put_le(hello + 12, 0x81000000u);
    put_le(hello + 16, 0x02000049u); /* MOVI R1, 'I' */
    put_le(hello + 20, 0x81000000u);
    put_le(hello + 24, 0x02000046u); /* MOVI R1, 'F' */
    put_le(hello + 28, 0x81000000u);
    put_le(hello + 32, 0x0200000Au); /* MOVI R1, '\n' */
    put_le(hello + 36, 0x81000000u);
    put_le(hello + 40, 0xFF000000u); /* HLT */

    if (emit("examples/add.afbin", add, sizeof add) != 0)
        return 1;
    if (emit("examples/hello.afbin", hello, sizeof hello) != 0)
        return 1;
    if (emit("tests/add.afbin", add, sizeof add) != 0)
        return 1;
    return 0;
}
