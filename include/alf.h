#ifndef ALF_H
#define ALF_H

/*
 * On-disk .afbin image (little-endian fields). See docs/ISA.md §10.
 *
 *   0x00  4  magic     'A' 'L' 'I' 'F'
 *   0x04  2  ver_major 1
 *   0x06  2  ver_minor 0
 *   0x08  4  entry     byte offset into code (4-aligned)
 *   0x0C  4  code_size bytes (multiple of 4, > 0)
 *   0x10  4  data_size bytes (multiple of 4, copied to ram[0])
 *   0x14  4  reserved  0
 *   0x18  8  reserved  0
 *   0x20  code_size    instruction payload
 *   +     data_size    initial RAM image
 *
 * Host filename extension is ".afbin". Magic bytes stay "ALIF".
 */

#include "alif.h"

#define ALF_HEADER_BYTES        32
#define ALF_VERSION_MAJOR       1
#define ALF_VERSION_MINOR       0
#define ALF_MAX_CODE            (16u * 1024u * 1024u)
#define ALF_EXT                 ".afbin"

struct alif_image {
    unsigned char  *code;
    size_t          code_len;
    unsigned char   data[ALIF_RAM_SIZE];
    size_t          data_len;
    unsigned int    entry;
};

/* Returns NULL on success, a static error string on failure. */
const char *alif_load_afbin(const char *path, struct alif_image *img);
void        alif_image_free(struct alif_image *img);

int         alif_write_afbin(const char *path,
                             const unsigned char *code, size_t code_len,
                             const unsigned char *data, size_t data_len,
                             unsigned int entry);

#endif /* ALF_H */
