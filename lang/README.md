# علیف — source language (v1)

This is the **programming language** layer. It is independent of the VM.

```
.alif   (AF8 bytes, Urdu source)     language  — this folder
   |
   |  future compiler (not built)
   v
.afb    (ASCII assembly)             afas/
   |
   |  afas
   v
.afbin  (machine image)              alif.exe
   |
   |  VM
```

`alif.exe` and `afas` do not read `.alif` files. Transcoders/editors for showing AF8 as Urdu glyphs are **later**. Specs here use Unicode so humans can read the grammar; on disk a `.alif` file is **AF8** (`af8/`).

---

## What v1 is

A tiny **imperative** language: 32-bit integers, variables, `if` / `else` / `while`, print/read, arithmetic. No functions, no heap, no strings (yet). It is meant to lower onto the existing ISA (`MOVI`, `ADD`, `CMP`, `Jcc`, `OUT`, `IN`, `HLT`).

---

## Keywords (reserved)

Written as one word, no space inside. Compared as AF8 letter bytes (`lang/keywords.h`).

| Urdu | Role | ASCII analogue |
|---|---|---|
| `شروع` | program start | `main {` |
| `ختم` | program end | `}` of main |
| `عدد` | integer binding | `int` |
| `اگر` | if | `if` |
| `ورنہ` | else | `else` |
| `جبتک` | while | `while` |
| `لکھو` | print number + newline | `OUT 1` |
| `پڑھو` | read integer into a variable | `IN` (v1: port 0 byte; see notes) |
| `اور` | logical and | `&&` |
| `یا` | logical or | `\|\|` |
| `نہیں` | logical not | `!` |
| `سچ` | true (integer 1) | `true` |
| `جھوٹ` | false (integer 0) | `false` |

These identifiers cannot be variable names.

---

## Example (logical source)

Same meaning as `examples/add.afb` (`2 + 6`, print):

```
شروع
    عدد الف = 2؛
    عدد ب = 6؛
    عدد جواب = الف + ب؛
    لکھو جواب؛
ختم
```

If:

```
شروع
    عدد ن = 3؛
    اگر ن > 0 {
        لکھو ن؛
    } ورنہ {
        لکھو 0؛
    }
ختم
```

Loop:

```
شروع
    عدد ن = 3؛
    جبتک ن > 0 {
        لکھو ن؛
        ن = ن - 1؛
    }
ختم
```

---

## Layout of a file

1. Optional `//` comments (ASCII), blank lines, spaces/tabs.
2. Exactly one program: `شروع` … statements … `ختم`.
3. Nothing after `ختم` except comments/whitespace.

Harakat (`َُِ` etc.) are **not** part of v1 tokens: a well-formed `.alif` should omit them. If present, a future lexer may strip `AF8_IS_HARAKAT`.

Statement terminator is **Urdu semicolon** `؛` (`AF8_SEMICOLON`, `0xB1`). ASCII `;` is an accepted alias.

Blocks use ASCII `{` `}`.

---

## Grammar (EBNF)

Whitespace is ignored between tokens. `ident` and keywords are longest-match AF8 letter runs (`AF8_IS_LETTER`).

```
program     = "شروع" { stmt } "ختم" ;

stmt        = decl
            | assign
            | print
            | read
            | if
            | while
            ;

decl        = "عدد" ident "=" expr "؛" ;
assign      = ident "=" expr "؛" ;
print       = "لکھو" expr "؛" ;
read        = "پڑھو" ident "؛" ;

if          = "اگر" expr block [ "ورنہ" block ] ;
while       = "جبتک" expr block ;
block       = "{" { stmt } "}" ;

expr        = or ;
or          = and { "یا" and } ;
and         = cmp { "اور" cmp } ;
cmp         = add { ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) add } ;
add         = mul { ( "+" | "-" ) mul } ;
mul         = unary { ( "*" | "/" | "%" ) unary } ;
unary       = "نہیں" unary | "+" unary | "-" unary | primary ;
primary     = number | "سچ" | "جھوٹ" | ident | "(" expr ")" ;

ident       = letter { letter | urdu_digit | ascii_digit } ;
letter      = (* AF8 0x80–0xAA *) ;
number      = ascii_digits | urdu_digits ;
ascii_digits= "0"…"9" { "0"…"9" } ;
urdu_digits = (* AF8 0xC0–0xC9 *) { … } ;
```

No mixing of ASCII and Urdu digits inside one number. `12` and `۱۲` are both twelve; `1۲` is illegal.

---

## Operators (ASCII, from the unused-free 7-bit half of AF8)

| Token | Meaning |
|---|---|
| `=` | assignment (in `decl` / `assign` only) |
| `+` `-` `*` `/` `%` | arithmetic (`/` and `%` unsigned, same as the ISA) |
| `==` `!=` `<` `>` `<=` `>=` | compare → `0` or `1` |
| `(` `)` | grouping |
| `{` `}` | block |
| `؛` or `;` | end of statement |

`اور` / `یا` / `نہیں` are **short-circuit** in the compiler later (`اگر` style): `یا` stops on first nonzero, `اور` on first zero.

---

## Types and names

v1 has one type: **32-bit integer** (same as a VM register / RAM word).

`سچ` is `1`, `جھوٹ` is `0`. `اگر` / `جبتک` treat **nonzero as true**.

A name is one or more AF8 letters, then letters or digits. Keywords are reserved.

Suggested (not enforced) short names: `الف` `ب` `ج` `ن` `جواب`.

---

## Semantics (for the future compiler)

Informal lowering onto today’s ISA — not implemented here:

| Source | Assembly idea |
|---|---|
| `عدد الف = 2؛` | `MOVI` into a slot (register or RAM) |
| `الف + ب` | `ADD` |
| `لکھو جواب؛` | `OUT 1, rs` |
| `اگر expr { A } ورنہ { B }` | evaluate expr, `JZ` to B, A, `JMP` past B, B |
| `جبتک expr { A }` | label, expr, `JZ` exit, A, `JMP` label |
| end of `ختم` | `HLT` |

Variables: compiler-chosen; v1 can keep locals in RAM (`LOAD`/`STORE`) so more than 8 names are possible. Registers are an optimization.

`پڑھو ن؛` — v1 reads **one byte** from port 0 (`IN`) and zero-extends into `ن`. A later language version can define decimal input on another port.

Division by zero is the VM’s `FAULT_DIV0`.

---

## What v1 does not have

Functions, arrays, strings, `break`/`continue`, floats, modules, pointers. Add those in a later language revision without changing `.afb` / `.afbin` unless the ISA grows.

---

## Files

| File | Role |
|---|---|
| this README | overview |
| [`GRAMMAR.md`](GRAMMAR.md) | token rules + keyword AF8 bytes |
| [`keywords.h`](keywords.h) | the same words as C macros for a future compiler |
