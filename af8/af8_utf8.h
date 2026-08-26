#ifndef AF8_UTF8_H
#define AF8_UTF8_H

/*
 * UTF-8 ↔ AF8 for editor-facing .alif files.
 * alifc accepts UTF-8 (what a text editor writes) or raw AF8.
 * Not used by the VM or afas.
 */

#include "af8_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef AF8_UTF8_ENCODE
static uint32_t af8_uc_from_byte(unsigned char b)
{
    size_t i;
    if (b <= AF8_ASCII_MAX)
        return (uint32_t)b;
    for (i = 0; i < AF8_UC_COUNT; i++) {
        if (af8_uc_table[i].af8 == b)
            return af8_uc_table[i].uc;
    }
    return 0;
}

static int af8_utf8_put(unsigned char *o, uint32_t cp)
{
    if (cp < 0x80) {
        o[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800) {
        o[0] = (unsigned char)(0xC0 | (cp >> 6));
        o[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = (unsigned char)(0xE0 | (cp >> 12));
        o[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    o[0] = (unsigned char)(0xF0 | (cp >> 18));
    o[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    o[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}
#else

static int af8_byte_from_uc(uint32_t uc, unsigned char *out)
{
    size_t i;
    if (uc <= AF8_ASCII_MAX) {
        *out = (unsigned char)uc;
        return 1;
    }
    for (i = 0; i < AF8_UC_COUNT; i++) {
        if (af8_uc_table[i].uc == uc) {
            *out = af8_uc_table[i].af8;
            return 1;
        }
    }
    return 0;
}

static int af8_utf8_next(const unsigned char *s, size_t n, size_t *i, uint32_t *cp)
{
    unsigned char b0, b1, b2, b3;
    uint32_t c;
    size_t p = *i;

    if (p >= n)
        return 0;
    b0 = s[p];
    if (b0 < 0x80) {
        *cp = b0;
        *i = p + 1;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) {
        if (p + 1 >= n)
            return -1;
        b1 = s[p + 1];
        if ((b1 & 0xC0) != 0x80)
            return -1;
        c = ((uint32_t)(b0 & 0x1F) << 6) | (uint32_t)(b1 & 0x3F);
        if (c < 0x80)
            return -1;
        *cp = c;
        *i = p + 2;
        return 1;
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (p + 2 >= n)
            return -1;
        b1 = s[p + 1];
        b2 = s[p + 2];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
            return -1;
        c = ((uint32_t)(b0 & 0x0F) << 12)
          | ((uint32_t)(b1 & 0x3F) << 6)
          | (uint32_t)(b2 & 0x3F);
        if (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF))
            return -1;
        *cp = c;
        *i = p + 3;
        return 1;
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (p + 3 >= n)
            return -1;
        b1 = s[p + 1];
        b2 = s[p + 2];
        b3 = s[p + 3];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
            return -1;
        c = ((uint32_t)(b0 & 0x07) << 18)
          | ((uint32_t)(b1 & 0x3F) << 12)
          | ((uint32_t)(b2 & 0x3F) << 6)
          | (uint32_t)(b3 & 0x3F);
        if (c < 0x10000 || c > 0x10FFFF)
            return -1;
        *cp = c;
        *i = p + 4;
        return 1;
    }
    return -1;
}

static int af8_looks_like_utf8(const unsigned char *s, size_t n)
{
    size_t i;
    if (n >= 3 && s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF)
        return 1;
    for (i = 0; i < n; i++) {
        unsigned char b = s[i];
        uint32_t cp;
        size_t j;
        if (b < 0x80)
            continue;
        /* AF8 letters are 0x80–0xAA: UTF-8 continuation bytes, not starters. */
        j = i;
        if (af8_utf8_next(s, n, &j, &cp) == 1)
            return 1;
        return 0;
    }
    return 0;
}

static int af8_is_format_uc(uint32_t cp)
{
    if (cp == 0xFEFF)
        return 1;
    if (cp >= 0x200B && cp <= 0x200F)
        return 1;
    if (cp >= 0x202A && cp <= 0x202E)
        return 1;
    if (cp >= 0x2060 && cp <= 0x2064)
        return 1;
    if (cp >= 0x2066 && cp <= 0x2069)
        return 1;
    return 0;
}

/*
 * Convert UTF-8 src to a freshly malloc'd AF8 buffer (*dst, *dn).
 * Returns 0 on success. On error, fills err and returns -1; *dst is not set.
 */
static int af8_utf8_to_af8(const unsigned char *src, size_t n,
                           unsigned char **dst, size_t *dn,
                           char *err, size_t errn)
{
    unsigned char *out;
    size_t i = 0, o = 0;
    uint32_t cp;
    int r;

    if (n >= 3 && src[0] == 0xEF && src[1] == 0xBB && src[2] == 0xBF)
        i = 3;

    out = (unsigned char *)malloc(n + 1);
    if (out == NULL) {
        snprintf(err, errn, "out of memory");
        return -1;
    }

    while (i < n) {
        unsigned char b;
        r = af8_utf8_next(src, n, &i, &cp);
        if (r != 1) {
            free(out);
            snprintf(err, errn, "invalid UTF-8");
            return -1;
        }
        if (cp == 0xFFFD) {
            free(out);
            snprintf(err, errn,
                "U+FFFD replacement character: this file is not Urdu. "
                "An editor opened AF8 bytes as UTF-8 and destroyed them. "
                "Save the program as UTF-8 Urdu, or regenerate samples "
                "with alifc\\mk_samples.exe");
            return -1;
        }
        if (af8_is_format_uc(cp))
            continue;
        if (!af8_byte_from_uc(cp, &b)) {
            free(out);
            snprintf(err, errn, "unmapped Unicode U+%04X", cp);
            return -1;
        }
        out[o++] = b;
    }
    out[o] = 0;
    *dst = out;
    *dn = o;
    return 0;
}

#endif /* !AF8_UTF8_ENCODE */

#endif /* AF8_UTF8_H */
