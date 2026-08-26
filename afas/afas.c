#include "alf.h"
#include "opcodes.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * afas — ALIF assembler
 *
 * Reads .afb (assembly), writes .afbin (the same image alif.exe already runs).
 * The VM and launcher do not know this tool exists.
 */

#define AFB_EXT         ".afb"
#define MAX_LINE        1024
#define MAX_NAME        64
#define MAX_INSNS       65536
#define MAX_LABELS      4096

enum form {
    F_NONE,
    F_RD,
    F_RS1,
    F_RD_RS1,
    F_RD_RS1_RS2,
    F_RD_IMM,
    F_RD_RS1_IMM,
    F_RS1_RS2,
    F_RS1_IMM,
    F_RD_MEM,
    F_ST_MEM,
    F_J,
    F_RD_PORT,
    F_PORT_RS1,
    F_TRAP
};

struct mnem {
    const char *name;
    unsigned    op;
    enum form   form;
};

struct label {
    char     name[MAX_NAME];
    uint32_t addr;
    int      line;
};

struct insn {
    int         line;
    unsigned    op;
    enum form   form;
    int         rd, rs1, rs2;
    int32_t     imm;
    int         uses_label;
    char        target[MAX_NAME];
};

static const struct mnem k_mnem[] = {
    { "NOP",  OP_NOP,  F_NONE },
    { "HLT",  OP_HLT,  F_NONE },
    { "RET",  OP_RET,  F_NONE },
    { "INC",  OP_INC,  F_RD },
    { "DEC",  OP_DEC,  F_RD },
    { "POP",  OP_POP,  F_RD },
    { "PUSH", OP_PUSH, F_RS1 },
    { "MOV",  OP_MOV,  F_RD_RS1 },
    { "XCHG", OP_XCHG, F_RD_RS1 },
    { "NEG",  OP_NEG,  F_RD_RS1 },
    { "NOT",  OP_NOT,  F_RD_RS1 },
    { "ADD",  OP_ADD,  F_RD_RS1_RS2 },
    { "SUB",  OP_SUB,  F_RD_RS1_RS2 },
    { "MUL",  OP_MUL,  F_RD_RS1_RS2 },
    { "DIV",  OP_DIV,  F_RD_RS1_RS2 },
    { "MOD",  OP_MOD,  F_RD_RS1_RS2 },
    { "AND",  OP_AND,  F_RD_RS1_RS2 },
    { "OR",   OP_OR,   F_RD_RS1_RS2 },
    { "XOR",  OP_XOR,  F_RD_RS1_RS2 },
    { "SHL",  OP_SHL,  F_RD_RS1_RS2 },
    { "SHR",  OP_SHR,  F_RD_RS1_RS2 },
    { "SAR",  OP_SAR,  F_RD_RS1_RS2 },
    { "MOVI", OP_MOVI, F_RD_IMM },
    { "ADDI", OP_ADDI, F_RD_RS1_IMM },
    { "SUBI", OP_SUBI, F_RD_RS1_IMM },
    { "CMP",  OP_CMP,  F_RS1_RS2 },
    { "TEST", OP_TEST, F_RS1_RS2 },
    { "CMPI", OP_CMPI, F_RS1_IMM },
    { "LOAD", OP_LOAD, F_RD_MEM },
    { "STORE",OP_STORE,F_ST_MEM },
    { "JMP",  OP_JMP,  F_J },
    { "JZ",   OP_JZ,   F_J },
    { "JNZ",  OP_JNZ,  F_J },
    { "JE",   OP_JE,   F_J },
    { "JNE",  OP_JNE,  F_J },
    { "JL",   OP_JL,   F_J },
    { "JLE",  OP_JLE,  F_J },
    { "JG",   OP_JG,   F_J },
    { "JGE",  OP_JGE,  F_J },
    { "CALL", OP_CALL, F_J },
    { "IN",   OP_IN,   F_RD_PORT },
    { "OUT",  OP_OUT,  F_PORT_RS1 },
    { "TRAP", OP_TRAP, F_TRAP },
    { NULL,   0,       F_NONE }
};

static const char *g_src;
static struct insn  *g_insns;
static int           g_ninsns;
static struct label  g_labels[MAX_LABELS];
static int           g_nlabels;
static char          g_entry_name[MAX_NAME];
static int           g_entry_line;
static int           g_have_entry;

static int eq_ci(char a, char b)
{
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    return a == b;
}

static int has_ext(const char *path, const char *ext)
{
    size_t n, i, e;

    if (path == NULL || ext == NULL)
        return 0;
    n = strlen(path);
    e = strlen(ext);
    if (n < e)
        return 0;
    for (i = 0; i < e; i++) {
        if (!eq_ci(path[n - e + i], ext[i]))
            return 0;
    }
    return 1;
}

static void die(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "afas:%s:%d: ", g_src ? g_src : "?", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void strip_comment(char *s)
{
    int in_q = 0;
    char *p;

    for (p = s; *p; p++) {
        if (*p == '\'' && (p == s || p[-1] != '\\'))
            in_q = !in_q;
        else if (*p == ';' && !in_q) {
            *p = '\0';
            break;
        }
    }
}

static char *skip_ws(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r')
        s++;
    return s;
}

static int is_ident0(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int is_ident(char c)
{
    return is_ident0(c) || (c >= '0' && c <= '9');
}

static int icmp(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int take_ident(char **ps, char *out, size_t outsz, int line)
{
    char *s = skip_ws(*ps);
    size_t n = 0;

    if (!is_ident0(*s))
        return 0;
    while (is_ident(*s)) {
        if (n + 1 >= outsz)
            die(line, "name too long");
        out[n++] = *s++;
    }
    out[n] = '\0';
    *ps = s;
    return 1;
}

static int parse_reg(char **ps, int line)
{
    char name[MAX_NAME];
    char *s;
    int n;

    if (!take_ident(ps, name, sizeof name, line))
        die(line, "expected register R1..R8");
    if ((name[0] != 'R' && name[0] != 'r') || name[1] == '\0' || name[2] != '\0')
        die(line, "expected register R1..R8, got '%s'", name);
    n = name[1] - '0';
    if (n < 1 || n > 8)
        die(line, "register must be R1..R8, got '%s'", name);
    s = skip_ws(*ps);
    *ps = s;
    return n - 1; /* encoding 0..7 */
}

static int parse_char(char **ps, int line, int32_t *out)
{
    char *s = skip_ws(*ps);
    int c;

    if (*s != '\'')
        return 0;
    s++;
    if (*s == '\\') {
        s++;
        switch (*s) {
        case 'n': c = 10; break;
        case 't': c = 9;  break;
        case 'r': c = 13; break;
        case '\\': c = '\\'; break;
        case '\'': c = '\''; break;
        case '0': c = 0; break;
        default:
            die(line, "unknown char escape");
            return 0;
        }
        s++;
    } else if (*s == '\0' || *s == '\'') {
        die(line, "empty char literal");
        return 0;
    } else {
        c = (unsigned char)*s++;
    }
    if (*s != '\'')
        die(line, "unterminated char literal");
    s++;
    *ps = skip_ws(s);
    *out = c;
    return 1;
}

static int parse_number(char **ps, int line, int32_t *out)
{
    char *s = skip_ws(*ps);
    int neg = 0;
    uint32_t v = 0;
    int digits = 0;

    if (parse_char(ps, line, out))
        return 1;

    if (*s == '+')
        s++;
    else if (*s == '-') {
        neg = 1;
        s++;
    }
    s = skip_ws(s);
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        while (isxdigit((unsigned char)*s)) {
            int d = *s;
            if (d >= '0' && d <= '9') d -= '0';
            else if (d >= 'a' && d <= 'f') d = d - 'a' + 10;
            else d = d - 'A' + 10;
            if (v > 0x0FFFFFFFu)
                die(line, "number too large");
            v = (v << 4) | (uint32_t)d;
            s++;
            digits++;
        }
    } else {
        while (*s >= '0' && *s <= '9') {
            if (v > 429496729u)
                die(line, "number too large");
            v = v * 10u + (uint32_t)(*s - '0');
            s++;
            digits++;
        }
    }
    if (digits == 0)
        return 0;
    *ps = skip_ws(s);
    if (neg) {
        if (v > 0x80000000u)
            die(line, "number too large");
        *out = v == 0x80000000u ? (int32_t)0x80000000 : -(int32_t)v;
    } else {
        *out = (int32_t)v;
    }
    return 1;
}

static void expect_comma(char **ps, int line)
{
    char *s = skip_ws(*ps);
    if (*s != ',')
        die(line, "expected ','");
    *ps = skip_ws(s + 1);
}

static void expect_end(char *s, int line)
{
    s = skip_ws(s);
    if (*s != '\0')
        die(line, "unexpected '%s'", s);
}

static void parse_mem(char **ps, int line, int *rs1, int32_t *disp)
{
    char *s = skip_ws(*ps);

    if (*s != '[')
        die(line, "expected memory operand [Rn] or [Rn+disp]");
    s++;
    *ps = s;
    *rs1 = parse_reg(ps, line);
    s = skip_ws(*ps);
    *disp = 0;
    if (*s == '+' || *s == '-') {
        int32_t n;
        int minus = (*s == '-');
        s++;
        *ps = s;
        if (!parse_number(ps, line, &n))
            die(line, "expected displacement");
        *disp = minus ? -n : n;
        s = skip_ws(*ps);
    }
    if (*s != ']')
        die(line, "expected ']'");
    *ps = skip_ws(s + 1);
}

static const struct mnem *find_mnem(const char *name)
{
    const struct mnem *m;
    for (m = k_mnem; m->name; m++) {
        if (icmp(m->name, name) == 0)
            return m;
    }
    return NULL;
}

static void add_label(const char *name, uint32_t addr, int line)
{
    int i;
    for (i = 0; i < g_nlabels; i++) {
        if (icmp(g_labels[i].name, name) == 0)
            die(line, "duplicate label '%s' (also line %d)", name, g_labels[i].line);
    }
    if (g_nlabels >= MAX_LABELS)
        die(line, "too many labels");
    if (strlen(name) >= MAX_NAME)
        die(line, "label too long");
    memcpy(g_labels[g_nlabels].name, name, strlen(name) + 1);
    g_labels[g_nlabels].addr = addr;
    g_labels[g_nlabels].line = line;
    g_nlabels++;
}

static uint32_t lookup_label(const char *name, int line)
{
    int i;
    for (i = 0; i < g_nlabels; i++) {
        if (icmp(g_labels[i].name, name) == 0)
            return g_labels[i].addr;
    }
    die(line, "undefined label '%s'", name);
    return 0;
}

static void parse_jump_target(char **ps, int line, struct insn *in)
{
    char name[MAX_NAME];
    int32_t n;

    if (take_ident(ps, name, sizeof name, line)) {
        if (name[0] == 'R' || name[0] == 'r') {
            if (name[1] >= '1' && name[1] <= '8' && name[2] == '\0')
                die(line, "jump target must be a label or address, not a register");
        }
        in->uses_label = 1;
        memcpy(in->target, name, strlen(name) + 1);
        return;
    }
    if (!parse_number(ps, line, &n))
        die(line, "expected label or address");
    in->uses_label = 0;
    in->imm = n;
}

static void parse_operands(char **ps, int line, struct insn *in, enum form form)
{
    switch (form) {
    case F_NONE:
        break;
    case F_RD:
        in->rd = parse_reg(ps, line);
        break;
    case F_RS1:
        in->rs1 = parse_reg(ps, line);
        break;
    case F_RD_RS1:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        in->rs1 = parse_reg(ps, line);
        break;
    case F_RD_RS1_RS2:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        in->rs1 = parse_reg(ps, line);
        expect_comma(ps, line);
        in->rs2 = parse_reg(ps, line);
        break;
    case F_RD_IMM:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected immediate");
        break;
    case F_RD_RS1_IMM:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        in->rs1 = parse_reg(ps, line);
        expect_comma(ps, line);
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected immediate");
        break;
    case F_RS1_RS2:
        in->rs1 = parse_reg(ps, line);
        expect_comma(ps, line);
        in->rs2 = parse_reg(ps, line);
        break;
    case F_RS1_IMM:
        in->rs1 = parse_reg(ps, line);
        expect_comma(ps, line);
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected immediate");
        break;
    case F_RD_MEM:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        parse_mem(ps, line, &in->rs1, &in->imm);
        break;
    case F_ST_MEM:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        parse_mem(ps, line, &in->rs1, &in->imm);
        break;
    case F_J:
        parse_jump_target(ps, line, in);
        break;
    case F_RD_PORT:
        in->rd = parse_reg(ps, line);
        expect_comma(ps, line);
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected port number");
        break;
    case F_PORT_RS1:
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected port number");
        expect_comma(ps, line);
        in->rs1 = parse_reg(ps, line);
        break;
    case F_TRAP:
        if (!parse_number(ps, line, &in->imm))
            die(line, "expected trap number");
        break;
    }
}

static uint32_t u12(int32_t v, int line)
{
    if (v < -2048 || v > 2047)
        die(line, "disp12 out of range [-2048, 2047]");
    return (uint32_t)v & IMM12_MASK;
}

static uint32_t u16_unsigned(int32_t v, int line)
{
    if (v < 0 || v > 65535)
        die(line, "u16 out of range [0, 65535]");
    return (uint32_t)v & IMM16_MASK;
}

static uint32_t u16_signed(int32_t v, int line)
{
    if (v < -32768 || v > 32767)
        die(line, "imm16 out of range [-32768, 32767]");
    return (uint32_t)v & IMM16_MASK;
}

static uint32_t encode(const struct insn *in)
{
    uint32_t imm12, imm16, imm24;
    int line = in->line;

    switch (in->form) {
    case F_NONE:
        return ((uint32_t)in->op << OP_SHIFT);
    case F_RD:
        return ((uint32_t)in->op << OP_SHIFT) | ((uint32_t)in->rd << RD_SHIFT);
    case F_RS1:
        return ((uint32_t)in->op << OP_SHIFT) | ((uint32_t)in->rs1 << RS1_SHIFT);
    case F_RD_RS1:
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT);
    case F_RD_RS1_RS2:
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | ((uint32_t)in->rs2 << RS2_SHIFT);
    case F_RD_IMM:
        imm16 = u16_unsigned(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | imm16;
    case F_RD_RS1_IMM:
        imm16 = u16_signed(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | imm16;
    case F_RS1_RS2:
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | ((uint32_t)in->rs2 << RS2_SHIFT);
    case F_RS1_IMM:
        imm16 = u16_signed(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | imm16;
    case F_RD_MEM:
    case F_ST_MEM:
        imm12 = u12(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | imm12;
    case F_J:
        if (in->imm < 0 || (uint32_t)in->imm > IMM24_MASK)
            die(line, "jump address out of 24-bit range");
        if ((in->imm & 3) != 0)
            die(line, "jump address must be 4-aligned");
        imm24 = (uint32_t)in->imm & IMM24_MASK;
        return ((uint32_t)in->op << OP_SHIFT) | imm24;
    case F_RD_PORT:
        imm16 = u16_unsigned(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rd << RD_SHIFT)
             | imm16;
    case F_PORT_RS1:
        imm16 = u16_unsigned(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT)
             | ((uint32_t)in->rs1 << RS1_SHIFT)
             | imm16;
    case F_TRAP:
        imm16 = u16_unsigned(in->imm, line);
        return ((uint32_t)in->op << OP_SHIFT) | imm16;
    }
    die(line, "internal form error");
    return 0;
}

static void parse_line(char *line, int lineno)
{
    char *s = skip_ws(line);
    char ident[MAX_NAME];
    const struct mnem *m;
    struct insn *in;

    if (*s == '\0')
        return;

    if (*s == '.') {
        s++;
        if (!take_ident(&s, ident, sizeof ident, lineno))
            die(lineno, "expected directive name");
        if (icmp(ident, "entry") == 0) {
            if (!take_ident(&s, g_entry_name, sizeof g_entry_name, lineno))
                die(lineno, ".entry needs a label");
            g_have_entry = 1;
            g_entry_line = lineno;
            expect_end(s, lineno);
            return;
        }
        die(lineno, "unknown directive '.%s'", ident);
    }

    if (!take_ident(&s, ident, sizeof ident, lineno))
        die(lineno, "expected label or mnemonic");

    s = skip_ws(s);
    if (*s == ':') {
        uint32_t addr = (uint32_t)g_ninsns * (uint32_t)ALIF_INSN_BYTES;
        add_label(ident, addr, lineno);
        s = skip_ws(s + 1);
        if (*s == '\0')
            return;
        if (!take_ident(&s, ident, sizeof ident, lineno))
            die(lineno, "expected mnemonic after label");
        s = skip_ws(s);
    }

    m = find_mnem(ident);
    if (m == NULL)
        die(lineno, "unknown mnemonic '%s'", ident);

    if (g_ninsns >= MAX_INSNS)
        die(lineno, "too many instructions");

    in = &g_insns[g_ninsns];
    memset(in, 0, sizeof *in);
    in->line = lineno;
    in->op = m->op;
    in->form = m->form;
    parse_operands(&s, lineno, in, m->form);
    expect_end(s, lineno);
    g_ninsns++;
}

static char *derive_out(const char *in)
{
    size_t n = strlen(in);
    size_t e = strlen(AFB_EXT);
    char *out;
    size_t base;

    if (n >= e && has_ext(in, AFB_EXT))
        base = n - e;
    else
        base = n;
    out = (char *)malloc(base + strlen(ALF_EXT) + 1);
    if (out == NULL) {
        fprintf(stderr, "afas: out of memory\n");
        exit(1);
    }
    memcpy(out, in, base);
    memcpy(out + base, ALF_EXT, strlen(ALF_EXT) + 1);
    return out;
}

static void usage(void)
{
    fprintf(stderr, "usage: afas <program.afb> [-o <program.afbin>]\n");
    exit(1);
}

int main(int argc, char **argv)
{
    const char *in_path = NULL;
    const char *out_path = NULL;
    char *derived = NULL;
    FILE *fp;
    char line[MAX_LINE];
    int lineno = 0;
    int i;
    unsigned char *code;
    size_t code_len;
    unsigned int entry = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc)
                usage();
            out_path = argv[++i];
        } else if (argv[i][0] == '-') {
            usage();
        } else if (in_path == NULL) {
            in_path = argv[i];
        } else {
            usage();
        }
    }
    if (in_path == NULL)
        usage();
    if (!has_ext(in_path, AFB_EXT)) {
        fprintf(stderr, "afas: source must be a .afb file\n");
        usage();
    }
    if (out_path == NULL) {
        derived = derive_out(in_path);
        out_path = derived;
    } else if (!has_ext(out_path, ALF_EXT)) {
        fprintf(stderr, "afas: output must be a .afbin file\n");
        usage();
    }

    g_src = in_path;
    g_insns = (struct insn *)calloc((size_t)MAX_INSNS, sizeof(struct insn));
    if (g_insns == NULL) {
        fprintf(stderr, "afas: out of memory\n");
        return 1;
    }

    fp = fopen(in_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "afas: cannot open %s\n", in_path);
        return 1;
    }
    while (fgets(line, (int)sizeof line, fp) != NULL) {
        size_t n;
        lineno++;
        n = strlen(line);
        if (n + 1 >= sizeof line && line[n - 1] != '\n')
            die(lineno, "line too long");
        if (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n && line[n - 1] == '\r')
            line[--n] = '\0';
        strip_comment(line);
        parse_line(line, lineno);
    }
    fclose(fp);

    if (g_ninsns == 0)
        die(lineno ? lineno : 1, "no instructions");

    for (i = 0; i < g_ninsns; i++) {
        if (g_insns[i].uses_label)
            g_insns[i].imm = (int32_t)lookup_label(g_insns[i].target, g_insns[i].line);
    }
    if (g_have_entry)
        entry = lookup_label(g_entry_name, g_entry_line);

    code_len = (size_t)g_ninsns * (size_t)ALIF_INSN_BYTES;
    if (code_len > (size_t)ALF_MAX_CODE)
        die(lineno, "code exceeds 16 MiB");
    code = (unsigned char *)malloc(code_len);
    if (code == NULL) {
        fprintf(stderr, "afas: out of memory\n");
        return 1;
    }
    for (i = 0; i < g_ninsns; i++) {
        uint32_t w = encode(&g_insns[i]);
        size_t o = (size_t)i * 4u;
        code[o]     = (unsigned char)(w      );
        code[o + 1] = (unsigned char)(w >>  8);
        code[o + 2] = (unsigned char)(w >> 16);
        code[o + 3] = (unsigned char)(w >> 24);
    }

    if (alif_write_afbin(out_path, code, code_len, NULL, 0, entry) != 0) {
        fprintf(stderr, "afas: cannot write %s\n", out_path);
        return 1;
    }

    free(code);
    free(g_insns);
    free(derived);
    return 0;
}
