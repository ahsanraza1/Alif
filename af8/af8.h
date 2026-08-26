#ifndef AF8_H
#define AF8_H

/*
 * AF8 — ALIF 8-bit source encoding (compiler layer only)
 *
 * 0x00–0x7F  standard ASCII (unchanged)
 * 0x80–0xFF  Urdu letters, punctuation, digits, harakat
 *
 * A .alif file is a stream of these bytes. One Urdu letter = one byte.
 * VM / afas / alif.exe do not read this encoding.
 *
 * Spec: af8/README.md
 */

#define AF8_ASCII_MAX           0x7F

/* ---- letters 0x80–0xAA  (logical letters, not glyph forms) --------- */
#define AF8_ALEF                0x80    /* ا  U+0627 */
#define AF8_ALEF_MADDA          0x81    /* آ  U+0622 */
#define AF8_BEH                 0x82    /* ب  U+0628 */
#define AF8_PEH                 0x83    /* پ  U+067E */
#define AF8_TEH                 0x84    /* ت  U+062A */
#define AF8_TTEH                0x85    /* ٹ  U+0679 */
#define AF8_THEH                0x86    /* ث  U+062B */
#define AF8_JEEM                0x87    /* ج  U+062C */
#define AF8_TCHEH               0x88    /* چ  U+0686 */
#define AF8_HAH                 0x89    /* ح  U+062D */
#define AF8_KHAH                0x8A    /* خ  U+062E */
#define AF8_DAL                 0x8B    /* د  U+062F */
#define AF8_DDAL                0x8C    /* ڈ  U+0688 */
#define AF8_THAL                0x8D    /* ذ  U+0630 */
#define AF8_REH                 0x8E    /* ر  U+0631 */
#define AF8_RREH                0x8F    /* ڑ  U+0691 */
#define AF8_ZAIN                0x90    /* ز  U+0632 */
#define AF8_JEH                 0x91    /* ژ  U+0698 */
#define AF8_SEEN                0x92    /* س  U+0633 */
#define AF8_SHEEN               0x93    /* ش  U+0634 */
#define AF8_SAD                 0x94    /* ص  U+0635 */
#define AF8_DAD                 0x95    /* ض  U+0636 */
#define AF8_TAH                 0x96    /* ط  U+0637 */
#define AF8_ZAH                 0x97    /* ظ  U+0638 */
#define AF8_AIN                 0x98    /* ع  U+0639 */
#define AF8_GHAIN               0x99    /* غ  U+063A */
#define AF8_FEH                 0x9A    /* ف  U+0641 */
#define AF8_QAF                 0x9B    /* ق  U+0642 */
#define AF8_KAF                 0x9C    /* ک  U+06A9 */
#define AF8_GAF                 0x9D    /* گ  U+06AF */
#define AF8_LAM                 0x9E    /* ل  U+0644 */
#define AF8_MEEM                0x9F    /* م  U+0645 */
#define AF8_NOON                0xA0    /* ن  U+0646 */
#define AF8_NOON_GHUNNA         0xA1    /* ں  U+06BA */
#define AF8_WAW                 0xA2    /* و  U+0648 */
#define AF8_WAW_HAMZA           0xA3    /* ؤ  U+0624 */
#define AF8_HEH                 0xA4    /* ہ  U+06C1 */
#define AF8_HEH_DOCHASHM        0xA5    /* ھ  U+06BE */
#define AF8_HAMZA               0xA6    /* ء  U+0621 */
#define AF8_YEH                 0xA7    /* ی  U+06CC */
#define AF8_YEH_HAMZA           0xA8    /* ئ  U+0626 */
#define AF8_BARREE              0xA9    /* ے  U+06D2 */
#define AF8_BARREE_HAMZA        0xAA    /* ۓ  U+06D3 */

#define AF8_LETTER_MIN          0x80
#define AF8_LETTER_MAX          0xAA

/* ---- punctuation 0xB0–0xB7 ----------------------------------------- */
#define AF8_COMMA               0xB0    /* ،  U+060C */
#define AF8_SEMICOLON           0xB1    /* ؛  U+061B */
#define AF8_QUESTION            0xB2    /* ؟  U+061F */
#define AF8_STOP                0xB3    /* ۔  U+06D4 */
#define AF8_LDQUOTE             0xB4    /* «  U+00AB */
#define AF8_RDQUOTE             0xB5    /* »  U+00BB */
#define AF8_PERCENT             0xB6    /* ٪  U+066A */
#define AF8_TATWEEL             0xB7    /* ـ  U+0640 */

/* ---- Urdu digits 0xC0–0xC9 ----------------------------------------- */
#define AF8_DIGIT_0             0xC0    /* ۰  U+06F0 */
#define AF8_DIGIT_1             0xC1    /* ۱ */
#define AF8_DIGIT_2             0xC2    /* ۲ */
#define AF8_DIGIT_3             0xC3    /* ۳ */
#define AF8_DIGIT_4             0xC4    /* ۴ */
#define AF8_DIGIT_5             0xC5    /* ۵ */
#define AF8_DIGIT_6             0xC6    /* ۶ */
#define AF8_DIGIT_7             0xC7    /* ۷ */
#define AF8_DIGIT_8             0xC8    /* ۸ */
#define AF8_DIGIT_9             0xC9    /* ۹  U+06F9 */

/* ---- harakat 0xD0–0xD8 --------------------------------------------- */
#define AF8_FATHATAN            0xD0    /* ً  U+064B */
#define AF8_DAMMATAN            0xD1    /* ٌ  U+064C */
#define AF8_KASRATAN            0xD2    /* ٍ  U+064D */
#define AF8_FATHA               0xD3    /* َ  U+064E  zabar */
#define AF8_DAMMA               0xD4    /* ُ  U+064F  pesh  */
#define AF8_KASRA               0xD5    /* ِ  U+0650  zer   */
#define AF8_SHADDA              0xD6    /* ّ  U+0651 */
#define AF8_SUKUN               0xD7    /* ْ  U+0652  jazm  */
#define AF8_SUPERSCRIPT_ALEF    0xD8    /* ٰ  U+0670 */

/* 0xAB–0xAF, 0xB8–0xBF, 0xCA–0xCF, 0xD9–0xFE reserved for later.
 * 0xFF is not a character; a well-formed .alif file never contains it.
 */
#define AF8_INVALID             0xFF

#define AF8_IS_ASCII(c)         ((unsigned char)(c) <= AF8_ASCII_MAX)
#define AF8_IS_LETTER(c)        ((unsigned char)(c) >= AF8_LETTER_MIN \
                              && (unsigned char)(c) <= AF8_LETTER_MAX)
#define AF8_IS_URDU_DIGIT(c)    ((unsigned char)(c) >= AF8_DIGIT_0 \
                              && (unsigned char)(c) <= AF8_DIGIT_9)
#define AF8_IS_HARAKAT(c)       ((unsigned char)(c) >= AF8_FATHATAN \
                              && (unsigned char)(c) <= AF8_SUPERSCRIPT_ALEF)

#endif /* AF8_H */
