#ifndef ALIF_LANG_KEYWORDS_H
#define ALIF_LANG_KEYWORDS_H

/*
 * علیف source language — reserved words as AF8 byte sequences.
 * Grammar: lang/GRAMMAR.md
 * Encoding: af8/af8.h
 * Not used by the VM or afas.
 */

#include "../af8/af8.h"

#define KW_ADAD     AF8_AIN, AF8_DAL, AF8_DAL
#define KW_AGAR     AF8_ALEF, AF8_GAF, AF8_REH
#define KW_WARNA    AF8_WAW, AF8_REH, AF8_NOON, AF8_HEH
#define KW_JABTAK   AF8_JEEM, AF8_BEH, AF8_TEH, AF8_KAF
#define KW_LIKHO    AF8_LAM, AF8_KAF, AF8_HEH_DOCHASHM, AF8_WAW
#define KW_PARHO    AF8_PEH, AF8_RREH, AF8_HEH_DOCHASHM, AF8_WAW
#define KW_SHURU    AF8_SHEEN, AF8_REH, AF8_WAW, AF8_AIN
#define KW_KHATAM   AF8_KHAH, AF8_TEH, AF8_MEEM
#define KW_AUR      AF8_ALEF, AF8_WAW, AF8_REH
#define KW_YA       AF8_YEH, AF8_ALEF
#define KW_NAHIN    AF8_NOON, AF8_HEH, AF8_YEH, AF8_NOON_GHUNNA
#define KW_SACH     AF8_SEEN, AF8_TCHEH
#define KW_JHOOT    AF8_JEEM, AF8_HEH_DOCHASHM, AF8_WAW, AF8_TTEH

#define KW_LEN_ADAD     3
#define KW_LEN_AGAR     3
#define KW_LEN_WARNA    4
#define KW_LEN_JABTAK   4
#define KW_LEN_LIKHO    4
#define KW_LEN_PARHO    4
#define KW_LEN_SHURU    4
#define KW_LEN_KHATAM   3
#define KW_LEN_AUR      3
#define KW_LEN_YA       2
#define KW_LEN_NAHIN    4
#define KW_LEN_SACH     2
#define KW_LEN_JHOOT    4

#endif /* ALIF_LANG_KEYWORDS_H */
