#ifndef AF8_MAP_H
#define AF8_MAP_H

/*
 * Unicode → AF8 for a future transcoder (UTF-8 editor file → .alif bytes).
 * Aliases fold Arabic lookalikes onto Urdu letters.
 * Not used by the VM.
 */

#include "af8.h"

#include <stdint.h>

struct af8_uc_row {
    uint32_t      uc;
    unsigned char af8;
};

static const struct af8_uc_row af8_uc_table[] = {
    { 0x0627, AF8_ALEF },
    { 0x0622, AF8_ALEF_MADDA },
    { 0x0628, AF8_BEH },
    { 0x067E, AF8_PEH },
    { 0x062A, AF8_TEH },
    { 0x0679, AF8_TTEH },
    { 0x062B, AF8_THEH },
    { 0x062C, AF8_JEEM },
    { 0x0686, AF8_TCHEH },
    { 0x062D, AF8_HAH },
    { 0x062E, AF8_KHAH },
    { 0x062F, AF8_DAL },
    { 0x0688, AF8_DDAL },
    { 0x0630, AF8_THAL },
    { 0x0631, AF8_REH },
    { 0x0691, AF8_RREH },
    { 0x0632, AF8_ZAIN },
    { 0x0698, AF8_JEH },
    { 0x0633, AF8_SEEN },
    { 0x0634, AF8_SHEEN },
    { 0x0635, AF8_SAD },
    { 0x0636, AF8_DAD },
    { 0x0637, AF8_TAH },
    { 0x0638, AF8_ZAH },
    { 0x0639, AF8_AIN },
    { 0x063A, AF8_GHAIN },
    { 0x0641, AF8_FEH },
    { 0x0642, AF8_QAF },
    { 0x06A9, AF8_KAF },
    { 0x0643, AF8_KAF },          /* Arabic kaf → Urdu ک */
    { 0x06AF, AF8_GAF },
    { 0x0644, AF8_LAM },
    { 0x0645, AF8_MEEM },
    { 0x0646, AF8_NOON },
    { 0x06BA, AF8_NOON_GHUNNA },
    { 0x0648, AF8_WAW },
    { 0x0624, AF8_WAW_HAMZA },
    { 0x06C1, AF8_HEH },
    { 0x0647, AF8_HEH },          /* Arabic heh → Urdu ہ */
    { 0x06D5, AF8_HEH },          /* AE */
    { 0x06BE, AF8_HEH_DOCHASHM },
    { 0x0621, AF8_HAMZA },
    { 0x06CC, AF8_YEH },
    { 0x064A, AF8_YEH },          /* Arabic yeh → Urdu ی */
    { 0x0649, AF8_YEH },          /* alef maksura */
    { 0x0626, AF8_YEH_HAMZA },
    { 0x06D2, AF8_BARREE },
    { 0x06D3, AF8_BARREE_HAMZA },

    { 0x060C, AF8_COMMA },
    { 0x061B, AF8_SEMICOLON },
    { 0x061F, AF8_QUESTION },
    { 0x06D4, AF8_STOP },
    { 0x00AB, AF8_LDQUOTE },
    { 0x00BB, AF8_RDQUOTE },
    { 0x066A, AF8_PERCENT },
    { 0x0640, AF8_TATWEEL },

    { 0x06F0, AF8_DIGIT_0 },
    { 0x06F1, AF8_DIGIT_1 },
    { 0x06F2, AF8_DIGIT_2 },
    { 0x06F3, AF8_DIGIT_3 },
    { 0x06F4, AF8_DIGIT_4 },
    { 0x06F5, AF8_DIGIT_5 },
    { 0x06F6, AF8_DIGIT_6 },
    { 0x06F7, AF8_DIGIT_7 },
    { 0x06F8, AF8_DIGIT_8 },
    { 0x06F9, AF8_DIGIT_9 },
    { 0x0660, AF8_DIGIT_0 },      /* Arabic-Indic digits */
    { 0x0661, AF8_DIGIT_1 },
    { 0x0662, AF8_DIGIT_2 },
    { 0x0663, AF8_DIGIT_3 },
    { 0x0664, AF8_DIGIT_4 },
    { 0x0665, AF8_DIGIT_5 },
    { 0x0666, AF8_DIGIT_6 },
    { 0x0667, AF8_DIGIT_7 },
    { 0x0668, AF8_DIGIT_8 },
    { 0x0669, AF8_DIGIT_9 },

    { 0x064B, AF8_FATHATAN },
    { 0x064C, AF8_DAMMATAN },
    { 0x064D, AF8_KASRATAN },
    { 0x064E, AF8_FATHA },
    { 0x064F, AF8_DAMMA },
    { 0x0650, AF8_KASRA },
    { 0x0651, AF8_SHADDA },
    { 0x0652, AF8_SUKUN },
    { 0x0670, AF8_SUPERSCRIPT_ALEF },
};

#define AF8_UC_COUNT  (sizeof af8_uc_table / sizeof af8_uc_table[0])

#endif /* AF8_MAP_H */
