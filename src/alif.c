#include "alf.h"

#include <stdio.h>
#include <string.h>

static const char *fault_name(int code)
{
    switch (code) {
    case ALIF_FAULT_ILL:   return "ILL";
    case ALIF_FAULT_ALIGN: return "ALIGN";
    case ALIF_FAULT_MEM:   return "MEM";
    case ALIF_FAULT_DIV0:  return "DIV0";
    case ALIF_FAULT_STK:   return "STK";
    case ALIF_FAULT_IO:    return "IO";
    case ALIF_FAULT_STEP:  return "STEP";
    default:               return "UNKNOWN";
    }
}

static int eq_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z')
        a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z')
        b = (char)(b - 'A' + 'a');
    return a == b;
}

/* True if path ends with ".afbin" (any case). */
static int has_afbin_ext(const char *path)
{
    static const char ext[] = ALF_EXT;
    size_t n, i, e = sizeof ext - 1;

    if (path == NULL)
        return 0;
    n = strlen(path);
    if (n < e)
        return 0;
    for (i = 0; i < e; i++) {
        if (!eq_ci(path[n - e + i], ext[i]))
            return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    struct alif_image img;
    struct alif_vm vm;
    const char *err;
    int rc;
    int exit_code;

    if (argc != 2) {
        fprintf(stderr, "usage: alif <program.afbin>\n");
        return 1;
    }
    if (!has_afbin_ext(argv[1])) {
        fprintf(stderr, "alif: program must be a .afbin file\n");
        fprintf(stderr, "usage: alif <program.afbin>\n");
        return 1;
    }

    memset(&img, 0, sizeof img);
    err = alif_load_afbin(argv[1], &img);
    if (err != NULL) {
        fprintf(stderr, "alif: %s: %s\n", argv[1], err);
        return 1;
    }

    alif_vm_init(&vm);
    if (img.data_len > 0)
        memcpy(vm.ram, img.data, img.data_len);

    rc = alif_exec_from(&vm, img.code, img.code_len, img.entry);
    alif_image_free(&img);

    if (rc != ALIF_OK) {
        fprintf(stderr, "alif: fault %s at ip=%u (step %u)\n",
                fault_name(rc), vm.ip, vm.steps);
        return 2;
    }

    exit_code = (int)vm.trap_code;
    if (exit_code < 0 || exit_code > 255)
        exit_code = 255;
    return exit_code;
}
