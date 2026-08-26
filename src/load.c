#include "alf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16(const unsigned char *p)
{
    return (uint16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

static uint32_t rd_u32(const unsigned char *p)
{
    return  (uint32_t)p[0]
         | ((uint32_t)p[1] <<  8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void wr_u16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v      );
    p[1] = (unsigned char)(v >>  8);
}

static void wr_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v      );
    p[1] = (unsigned char)(v >>  8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static int valid_sizes(size_t code_len, size_t data_len, unsigned int entry)
{
    if (code_len == 0 || (code_len & 3u) != 0)
        return 0;
    if (code_len > (size_t)ALF_MAX_CODE)
        return 0;
    if ((data_len & 3u) != 0 || data_len > (size_t)ALIF_RAM_SIZE)
        return 0;
    if ((entry & 3u) != 0)
        return 0;
    if ((size_t)entry >= code_len || code_len - (size_t)entry < (size_t)ALIF_INSN_BYTES)
        return 0;
    return 1;
}

void alif_image_free(struct alif_image *img)
{
    if (img == NULL)
        return;
    free(img->code);
    img->code     = NULL;
    img->code_len = 0;
    img->data_len = 0;
    img->entry    = 0;
    memset(img->data, 0, sizeof img->data);
}

const char *alif_load_afbin(const char *path, struct alif_image *img)
{
    FILE *fp;
    unsigned char hdr[ALF_HEADER_BYTES];
    long file_len;
    uint16_t ver_maj, ver_min;
    uint32_t entry, code_size, data_size, reserved14;
    size_t want;
    size_t n;

    if (path == NULL || img == NULL)
        return "invalid argument";

    memset(img, 0, sizeof(*img));

    fp = fopen(path, "rb");
    if (fp == NULL)
        return (errno == ENOENT) ? "file not found" : "cannot open file";

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return "cannot seek file";
    }
    file_len = ftell(fp);
    if (file_len < (long)ALF_HEADER_BYTES) {
        fclose(fp);
        return "file too small to be an .afbin image";
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return "cannot seek file";
    }

    n = fread(hdr, 1, ALF_HEADER_BYTES, fp);
    if (n != (size_t)ALF_HEADER_BYTES) {
        fclose(fp);
        return "truncated header";
    }

    if (hdr[0] != 'A' || hdr[1] != 'L' || hdr[2] != 'I' || hdr[3] != 'F') {
        fclose(fp);
        return "bad magic (not an ALIF .afbin image)";
    }

    ver_maj     = rd_u16(hdr + 4);
    ver_min     = rd_u16(hdr + 6);
    entry       = rd_u32(hdr + 8);
    code_size   = rd_u32(hdr + 12);
    data_size   = rd_u32(hdr + 16);
    reserved14  = rd_u32(hdr + 20);
    if (reserved14 != 0 || rd_u32(hdr + 24) != 0 || rd_u32(hdr + 28) != 0) {
        fclose(fp);
        return "reserved header bytes must be zero";
    }
    if (ver_maj != (uint16_t)ALF_VERSION_MAJOR ||
        ver_min != (uint16_t)ALF_VERSION_MINOR) {
        fclose(fp);
        return "unsupported .afbin version (need 1.0)";
    }
    if (!valid_sizes((size_t)code_size, (size_t)data_size, entry)) {
        fclose(fp);
        return "invalid code/data size or entry IP";
    }

    want = (size_t)ALF_HEADER_BYTES + (size_t)code_size + (size_t)data_size;
    if ((unsigned long)file_len != (unsigned long)want) {
        fclose(fp);
        return "file size does not match header";
    }

    img->code = (unsigned char *)malloc((size_t)code_size);
    if (img->code == NULL) {
        fclose(fp);
        return "out of memory";
    }
    n = fread(img->code, 1, (size_t)code_size, fp);
    if (n != (size_t)code_size) {
        alif_image_free(img);
        fclose(fp);
        return "truncated code section";
    }
    if (data_size > 0) {
        n = fread(img->data, 1, (size_t)data_size, fp);
        if (n != (size_t)data_size) {
            alif_image_free(img);
            fclose(fp);
            return "truncated data section";
        }
    }
    fclose(fp);

    img->code_len = (size_t)code_size;
    img->data_len = (size_t)data_size;
    img->entry    = entry;
    return NULL;
}

int alif_write_afbin(const char *path,
                   const unsigned char *code, size_t code_len,
                   const unsigned char *data, size_t data_len,
                   unsigned int entry)
{
    FILE *fp;
    unsigned char hdr[ALF_HEADER_BYTES];

    if (path == NULL || code == NULL)
        return -1;
    if (data_len > 0 && data == NULL)
        return -1;
    if (!valid_sizes(code_len, data_len, entry))
        return -1;

    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'A';
    hdr[1] = 'L';
    hdr[2] = 'I';
    hdr[3] = 'F';
    wr_u16(hdr + 4, (uint16_t)ALF_VERSION_MAJOR);
    wr_u16(hdr + 6, (uint16_t)ALF_VERSION_MINOR);
    wr_u32(hdr + 8, (uint32_t)entry);
    wr_u32(hdr + 12, (uint32_t)code_len);
    wr_u32(hdr + 16, (uint32_t)data_len);

    fp = fopen(path, "wb");
    if (fp == NULL)
        return -1;
    if (fwrite(hdr, 1, ALF_HEADER_BYTES, fp) != (size_t)ALF_HEADER_BYTES ||
        fwrite(code, 1, code_len, fp) != code_len ||
        (data_len > 0 && fwrite(data, 1, data_len, fp) != data_len)) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0)
        return -1;
    return 0;
}
