# علیف v1 — tokens and keyword bytes

Human-readable grammar is in [`README.md`](README.md). This page is the **token contract** for a lexer: what bytes form a keyword vs an identifier.

On-disk `.alif` = AF8. Tables below show Unicode only for documentation.

---

## 1. Character classes (from `af8/af8.h`)

| Class | Bytes | Use |
|---|---|---|
| ASCII whitespace | `09` tab, `0A` LF, `0D` CR, `20` space | ignored between tokens |
| ASCII `//` | `2F 2F` … to end of line | comment |
| ASCII operators | see README | tokens |
| AF8 letter | `80`–`AA` | ident / keyword |
| ASCII digit | `30`–`39` | number |
| AF8 Urdu digit | `C0`–`C9` | number |
| AF8 semicolon | `B1` (`؛`) | statement end (alias ASCII `3B` `;`) |
| AF8 harakat | `D0`–`D8` | v1: illegal (or strip — compiler choice, pick one and keep it) |
| `FF` | | always illegal |
| other `AB–AF`, `B0`, `B2–B7`, `B8–BF`, `CA–CF`, `D9–FE` | | illegal in v1 except as listed |

`،` (`B0`) is **not** a comma in v1 (use ASCII `,` only if we add arg lists later). Do not treat `B0` as a token yet.

---

## 2. Maximal munch

- Read the longest operator: `<=` not `<` then `=`.
- Read the longest letter-run, then look it up in the keyword table. If it matches a row below, it is that keyword; else it is an `ident`.
- `جبتک` is one keyword (no space). `جب تک` is two idents — illegal as `while`.

---

## 3. Keyword ↔ AF8

Lengths and bytes must match `lang/keywords.h`.

| Keyword | Bytes (hex) | `af8.h` names |
|---|---|---|
| شروع | `93 8E A2 98` | SHEEN REH WAW AIN |
| ختم | `8A 84 9F` | KHAH TEH MEEM |
| عدد | `98 8B 8B` | AIN DAL DAL |
| اگر | `80 9D 8E` | ALEF GAF REH |
| ورنہ | `A2 8E A0 A4` | WAW REH NOON HEH |
| جبتک | `87 82 84 9C` | JEEM BEH TEH KAF |
| لکھو | `9E 9C A5 A2` | LAM KAF HEH_DOCHASHM WAW |
| پڑھو | `83 8F A5 A2` | PEH RREH HEH_DOCHASHM WAW |
| اور | `80 A2 8E` | ALEF WAW REH |
| یا | `A7 80` | YEH ALEF |
| نہیں | `A0 A4 A7 A1` | NOON HEH YEH NOON_GHUNNA |
| سچ | `92 88` | SEEN TCHEH |
| جھوٹ | `87 A5 A2 85` | JEEM HEH_DOCHASHM WAW TTEH |

---

## 4. Sample `add` as AF8 (no compiler yet)

Logical:

```
شروع
عدد الف = 2؛
عدد ب = 6؛
عدد جواب = الف + ب؛
لکھو جواب؛
ختم
```

`الف` = ALEF LAM FEH = `80 9E 9A`  
`ب` = BEH = `82`  
`جواب` = JEEM WAW ALEF BEH = `87 A2 80 82`

A lexer would emit tokens: `KW_SHURU`, `KW_ADAD`, `IDENT الف`, `=`, `NUM 2`, `؛`, …

The compiler (later) would emit assembly equivalent to `examples/add.afb`.

---

## 5. Lexer sketch (for later)

```
c = next byte
if AF8_IS_HARAKAT(c): error or skip   // v1: error
if c is space/tab/cr/lf: skip
if c == '/' and peek == '/': skip to LF
if c is ASCII operator start: match <= >= == != + - * / % ( ) { } = ;
if c == AF8_SEMICOLON: token SEMI
if AF8_IS_LETTER(c): gather letters/digits → keyword table or IDENT
if ASCII digit or AF8_IS_URDU_DIGIT: gather number
else: error
```

Do not UTF-8-decode. One byte is one AF8 character.
