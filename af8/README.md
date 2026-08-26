# AF8 — 8-bit Urdu encoding for `.alif` source

This folder is **not** part of the VM. `alif.exe`, `afas`, and `src/vm.c` stay ASCII/binary as they are.

Later:

```
programmer writes Urdu  →  file.alif  (AF8 bytes)
        |
        |  future compiler (not built yet)
        v
     file.afb           (existing assembler language)
        |
        |  afas
        v
     file.afbin
        |
        |  alif.exe
        v
        VM
```

---

## 1. Is this possible?

**Yes.** A programming language lexer in C reads `unsigned char`. That is already 8 bits, 256 values.

What people call “C cannot handle Urdu” is not a limit of `char`. It is **UTF-8 / UTF-16**:

| Encoding | Bytes per Urdu letter | C lexer |
|---|---|---|
| UTF-16 | 2 (what “2 bit per character” usually means as *2 bytes*) | must decode pairs |
| UTF-8 | 2–3 for Arabic/Urdu letters | must decode variable length |
| **AF8** | **1** | `c = fgetc();` is one letter |

ASCII graphic characters already occupy **0x20–0x7E** (space, digits `0–9`, `+ - * / = ( )` …). Those stay as they are so operators and ASCII digits need no new codes.

Slots that are **free for a private 8-bit set**:

| Range | Count | Role in AF8 |
|---|---|---|
| `0x00–0x7F` | 128 | unchanged ASCII (keep TAB, LF, CR, space, latin, ASCII digits) |
| `0x80–0xFF` | **128** | ours |

Urdu needs roughly:

- ~43 letters (ا … ے plus آ ؤ ئ ۓ ھ ں)
- 8 punctuation marks
- 10 digits ۰–۹
- 9 optional harakat (zabar/zer/…)

That is **about 70 codes**. 128 high bytes is enough. We do **not** steal unused C0 controls (`0x01–0x08` …) for letters: there are not enough of them, and they collide with file/C string conventions.

So: **8-bit AF8 is enough; 7-bit ASCII alone is not.**

This is the same idea as old DOS/Windows **code pages** (e.g. CP1256), but it is **our** page, named AF8, with Urdu letters in alphabet order.

### What we are explicitly ignoring (as requested)

Editors will show `0x80` as “Ç” or garbage unless they know AF8. That is fine for now. The compiler will treat the byte as `AF8_ALEF`, not as a glyph.

### What still has to happen later (not this step)

A `.alif` file **on disk is AF8**, not UTF-8. Notepad UTF-8 Urdu is a different byte sequence (`ا` is `D8 A7` in UTF-8, `80` in AF8). When the compiler is built, either:

- authors save/transcode into AF8, or
- the compiler accepts UTF-8 and maps through `af8_map.h` **before** lexing.

The language grammar is **not** defined here. Keywords will later be **sequences of AF8 letters**, not extra single-byte tokens.

---

## 2. Design rules

1. **ASCII 0–127 never changes.** Space, newline, `0`–`9`, `+`, `(`, `)` stay one byte so a future grammar can mix Urdu identifiers with ASCII operators.
2. **One logical letter, one byte.** Isolated/initial/medial/final shapes are display, not source. `ب` is always `0x82`.
3. **Urdu ک ی ہ**, not Arabic ك ي ه, are the canonical letters. The Unicode map aliases the Arabic forms onto the same AF8 bytes.
4. **Keywords are not assigned codes.** `اگر` will be three letter bytes, decided when we write the grammar.
5. **`0xFF` is illegal** in a well-formed `.alif` file (sentinel).
6. **Harakat are in the page** so a file can contain them; the grammar may later strip them in the lexer.

---

## 3. Code assignment

### 3.1 Letters (`0x80`–`0xAA`) — 43 codes

| AF8 | Glyph | Unicode | Macro |
|---|---|---|---|
| `80` | ا | U+0627 | `AF8_ALEF` |
| `81` | آ | U+0622 | `AF8_ALEF_MADDA` |
| `82` | ب | U+0628 | `AF8_BEH` |
| `83` | پ | U+067E | `AF8_PEH` |
| `84` | ت | U+062A | `AF8_TEH` |
| `85` | ٹ | U+0679 | `AF8_TTEH` |
| `86` | ث | U+062B | `AF8_THEH` |
| `87` | ج | U+062C | `AF8_JEEM` |
| `88` | چ | U+0686 | `AF8_TCHEH` |
| `89` | ح | U+062D | `AF8_HAH` |
| `8A` | خ | U+062E | `AF8_KHAH` |
| `8B` | د | U+062F | `AF8_DAL` |
| `8C` | ڈ | U+0688 | `AF8_DDAL` |
| `8D` | ذ | U+0630 | `AF8_THAL` |
| `8E` | ر | U+0631 | `AF8_REH` |
| `8F` | ڑ | U+0691 | `AF8_RREH` |
| `90` | ز | U+0632 | `AF8_ZAIN` |
| `91` | ژ | U+0698 | `AF8_JEH` |
| `92` | س | U+0633 | `AF8_SEEN` |
| `93` | ش | U+0634 | `AF8_SHEEN` |
| `94` | ص | U+0635 | `AF8_SAD` |
| `95` | ض | U+0636 | `AF8_DAD` |
| `96` | ط | U+0637 | `AF8_TAH` |
| `97` | ظ | U+0638 | `AF8_ZAH` |
| `98` | ع | U+0639 | `AF8_AIN` |
| `99` | غ | U+063A | `AF8_GHAIN` |
| `9A` | ف | U+0641 | `AF8_FEH` |
| `9B` | ق | U+0642 | `AF8_QAF` |
| `9C` | ک | U+06A9 | `AF8_KAF` |
| `9D` | گ | U+06AF | `AF8_GAF` |
| `9E` | ل | U+0644 | `AF8_LAM` |
| `9F` | م | U+0645 | `AF8_MEEM` |
| `A0` | ن | U+0646 | `AF8_NOON` |
| `A1` | ں | U+06BA | `AF8_NOON_GHUNNA` |
| `A2` | و | U+0648 | `AF8_WAW` |
| `A3` | ؤ | U+0624 | `AF8_WAW_HAMZA` |
| `A4` | ہ | U+06C1 | `AF8_HEH` |
| `A5` | ھ | U+06BE | `AF8_HEH_DOCHASHM` |
| `A6` | ء | U+0621 | `AF8_HAMZA` |
| `A7` | ی | U+06CC | `AF8_YEH` |
| `A8` | ئ | U+0626 | `AF8_YEH_HAMZA` |
| `A9` | ے | U+06D2 | `AF8_BARREE` |
| `AA` | ۓ | U+06D3 | `AF8_BARREE_HAMZA` |

`AF8_IS_LETTER(c)` is the closed range `0x80`–`0xAA`.

### 3.2 Punctuation (`0xB0`–`0xB7`)

| AF8 | Glyph | Unicode | Macro |
|---|---|---|---|
| `B0` | ، | U+060C | `AF8_COMMA` |
| `B1` | ؛ | U+061B | `AF8_SEMICOLON` |
| `B2` | ؟ | U+061F | `AF8_QUESTION` |
| `B3` | ۔ | U+06D4 | `AF8_STOP` |
| `B4` | « | U+00AB | `AF8_LDQUOTE` |
| `B5` | » | U+00BB | `AF8_RDQUOTE` |
| `B6` | ٪ | U+066A | `AF8_PERCENT` |
| `B7` | ـ | U+0640 | `AF8_TATWEEL` |

ASCII `, ; ? . " %` remain available too. Grammar will choose which form is official.

### 3.3 Digits (`0xC0`–`0xC9`)

Urdu digits ۰–۹. ASCII `0`–`9` (`0x30`–`0x39`) stay valid; grammar can accept both.

`AF8_DIGIT_n` = `0xC0 + n`.

### 3.4 Harakat (`0xD0`–`0xD8`)

zabar / pesh / zer / tashdid / jazm / tanween / superscript alif. Optional.

### 3.5 Reserved

`0xAB–0xAF`, `0xB8–0xBF`, `0xCA–0xCF`, `0xD9–0xFE` — unused, must not appear in v1 `.alif` files.

---

## 4. Example (bytes, not editor display)

ASCII `x` is `0x78`. Word `اب` would be two bytes `80 82` (`ا` then `ب`), not UTF-8 `D8 A7 D8 A8`.

A mixed line the grammar might allow later:

```
[AF8 letters for a keyword]  [ASCII space]  [ASCII 1]  [ASCII +]  [ASCII 2]
```

Each of those is still one `unsigned char`.

---

## 5. Files

| File | Role |
|---|---|
| [`af8.h`](af8.h) | numeric contract (`#define` only + classifiers) |
| [`af8_map.h`](af8_map.h) | Unicode → AF8 table for a future UTF-8 front end |
| this README | analysis and assignment |

The future compiler includes `af8.h` and lexes `unsigned char`. It emits **`.afb`** (existing assembly). It must not call the VM.

---

## 6. Next (not done)

1. Language grammar (Urdu keywords as AF8 letter sequences).
2. Compiler: `.alif` (AF8) → `.afb`.
3. Optional UTF-8 → AF8 filter using `af8_map.h`.
