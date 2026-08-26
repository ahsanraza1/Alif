#include "../lang/keywords.h"
#define AF8_UTF8_ENCODE
#include "../af8/af8_utf8.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

/* Write UTF-8 .alif samples (editors can open these). Run from repo root. */

static void wr(FILE *f, const unsigned char *p, int n)
{
    int i;
    unsigned char buf[4];
    for (i = 0; i < n; i++) {
        uint32_t uc = af8_uc_from_byte(p[i]);
        int k;
        if (uc == 0) {
            fprintf(stderr, "mk_samples: no Unicode for AF8 0x%02X\n", p[i]);
            exit(1);
        }
        k = af8_utf8_put(buf, uc);
        fwrite(buf, 1, (size_t)k, f);
    }
}

static void wch(FILE *f, unsigned char c)
{
    unsigned char buf[4];
    uint32_t uc = af8_uc_from_byte(c);
    int k;
    if (uc == 0 && c != 0) {
        fprintf(stderr, "mk_samples: no Unicode for AF8 0x%02X\n", c);
        exit(1);
    }
    k = af8_utf8_put(buf, uc);
    fwrite(buf, 1, (size_t)k, f);
}

static void wascii(FILE *f, const char *s)
{
    fputs(s, f);
}

int main(void)
{
    FILE *f;
    static const unsigned char shuru[]  = { KW_SHURU };
    static const unsigned char khatam[] = { KW_KHATAM };
    static const unsigned char adad[]   = { KW_ADAD };
    static const unsigned char likho[]  = { KW_LIKHO };
    static const unsigned char agar[]   = { KW_AGAR };
    static const unsigned char warna[]  = { KW_WARNA };
    static const unsigned char jabtak[] = { KW_JABTAK };
    static const unsigned char alif[]   = { AF8_ALEF, AF8_LAM, AF8_FEH };
    static const unsigned char be[]     = { AF8_BEH };
    static const unsigned char jawab[]  = { AF8_JEEM, AF8_WAW, AF8_ALEF, AF8_BEH };
    static const unsigned char noon[]   = { AF8_NOON };

    (void)MKDIR("alifc");
    (void)MKDIR("alifc/samples");

    f = fopen("alifc/samples/add.alif", "wb");
    if (f == NULL) {
        fprintf(stderr, "cannot write alifc/samples/add.alif\n");
        return 1;
    }
    wr(f, shuru, KW_LEN_SHURU); wch(f, '\n');
    wr(f, adad, KW_LEN_ADAD); wascii(f, " "); wr(f, alif, 3); wascii(f, " = 2"); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, adad, KW_LEN_ADAD); wascii(f, " "); wr(f, be, 1); wascii(f, " = 6"); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, adad, KW_LEN_ADAD); wascii(f, " "); wr(f, jawab, 4); wascii(f, " = "); wr(f, alif, 3); wascii(f, " + "); wr(f, be, 1); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, likho, KW_LEN_LIKHO); wascii(f, " "); wr(f, jawab, 4); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, khatam, KW_LEN_KHATAM); wch(f, '\n');
    fclose(f);

    f = fopen("alifc/samples/if.alif", "wb");
    if (f == NULL)
        return 1;
    wr(f, shuru, KW_LEN_SHURU); wch(f, '\n');
    wr(f, adad, KW_LEN_ADAD); wascii(f, " "); wr(f, noon, 1); wascii(f, " = 3"); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, agar, KW_LEN_AGAR); wascii(f, " "); wr(f, noon, 1); wascii(f, " > 0 {\n  ");
    wr(f, likho, KW_LEN_LIKHO); wascii(f, " "); wr(f, noon, 1); wch(f, AF8_SEMICOLON); wascii(f, "\n} ");
    wr(f, warna, KW_LEN_WARNA); wascii(f, " {\n  ");
    wr(f, likho, KW_LEN_LIKHO); wascii(f, " 0"); wch(f, AF8_SEMICOLON); wascii(f, "\n}\n");
    wr(f, khatam, KW_LEN_KHATAM); wch(f, '\n');
    fclose(f);

    f = fopen("alifc/samples/loop.alif", "wb");
    if (f == NULL)
        return 1;
    wr(f, shuru, KW_LEN_SHURU); wch(f, '\n');
    wr(f, adad, KW_LEN_ADAD); wascii(f, " "); wr(f, noon, 1); wascii(f, " = 3"); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, jabtak, KW_LEN_JABTAK); wascii(f, " "); wr(f, noon, 1); wascii(f, " > 0 {\n  ");
    wr(f, likho, KW_LEN_LIKHO); wascii(f, " "); wr(f, noon, 1); wch(f, AF8_SEMICOLON); wch(f, '\n');
    wr(f, noon, 1); wascii(f, " = "); wr(f, noon, 1); wascii(f, " - 1"); wch(f, AF8_SEMICOLON); wascii(f, "\n}\n");
    wr(f, khatam, KW_LEN_KHATAM); wch(f, '\n');
    fclose(f);

    return 0;
}
