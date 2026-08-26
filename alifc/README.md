# alifc — علیف compiler

Reads **`.alif`** and writes **`.afb`** (existing assembly). Does not run the VM. `afas` and `alif.exe` stay separate.

A `.alif` file is **UTF-8 Urdu** (what an editor saves) or raw **AF8**. `alifc` transcodes UTF-8 to AF8, then lexes. Do not open an AF8 byte file in a UTF-8 editor and save — that turns letters into U+FFFD and the source is lost.

```
.alif  →  alifc  →  .afb  →  afas  →  .afbin  →  alif.exe
```

## Build (gcc, no make)

From repo root:

```
build.bat alifc
```

or:

```
gcc -std=c11 -Wall -Wextra -Werror -o alifc/alifc.exe alifc/alifc.c
```

(`-I` not required; headers are included with `../lang/` and `../af8/`.)

## Run

```
alifc\alifc.exe alifc\samples\add.alif
afas\afas.exe alifc\samples\add.afb
alif.exe alifc\samples\add.afbin
```

After `build.bat`, the same chain is `ship\run.bat alifc\samples\add.alif`. `run.bat` only calls the three programs; it does not merge them.

Default output: same path with `.alif` replaced by `.afb`. `-o file.afb` to override.

| Exit | Meaning |
|---|---|
| 0 | wrote `.afb` |
| 1 | usage, I/O, or compile error (`alifc:file:line: …`) |

Source must end in `.alif`. Output must end in `.afb`.

## Samples

These are UTF-8 so a normal editor can show Urdu. From repo root you can also regenerate them:

```
gcc -std=c11 -Wall -Wextra -Werror -o alifc/mk_samples.exe alifc/mk_samples.c
alifc\mk_samples.exe
```

| File | Meaning | After `alif.exe` |
|---|---|---|
| `samples/add.alif` | 2 + 6, print | `8` |
| `samples/if.alif` | if n > 0 print n | `3` |
| `samples/loop.alif` | print 3, 2, 1 | three lines |

Language: [`../lang/README.md`](../lang/README.md). Encoding: [`../af8/README.md`](../af8/README.md).

Variables go in RAM (`LOAD`/`STORE`). Expression temps use `R1`/`R2` and `PUSH`/`POP`. End of program is `HLT`.
