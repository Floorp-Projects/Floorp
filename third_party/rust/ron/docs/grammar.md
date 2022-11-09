# RON grammar

This file describes the structure of a RON file in [EBNF notation][ebnf].
If extensions are enabled, some rules will be replaced. For that, see the
[extensions document][exts] which describes all extensions and what they override.

[ebnf]: https://en.wikipedia.org/wiki/Extended_Backus–Naur_form
[exts]: ./extensions.md

## RON file

```ebnf
RON = [extensions], ws, value, ws;
```

## Whitespace and comments

```ebnf
ws = { ws_single | comment };
ws_single = "\n" | "\t" | "\r" | " ";
comment = ["//", { no_newline }, "\n"] | ["/*", { ? any character ? }, "*/"];
```

## Commas

```ebnf
comma = ws, ",", ws;
```

## Extensions

```ebnf
extensions = { "#", ws, "!", ws, "[", ws, extensions_inner, ws, "]", ws };
extensions_inner = "enable", ws, "(", extension_name, { comma, extension_name }, [comma], ws, ")";
```

For the extension names see the [`extensions.md`][exts] document.

## Value

```ebnf
value = unsigned | signed | float | string | char | bool | option | list | map | tuple | struct | enum_variant;
```

## Numbers

```ebnf
digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9";
hex_digit = "A" | "a" | "B" | "b" | "C" | "c" | "D" | "d" | "E" | "e" | "F" | "f";
unsigned = (["0", ("b" | "o")], digit, { digit | '_' } |
             "0x", (digit | hex_digit), { digit | hex_digit | '_' });
signed = ["+" | "-"], unsigned;
float = ["+" | "-"], ("inf" | "NaN" | float_num);
float_num = (float_int | float_std | float_frac), [float_exp];
float_int = digit, { digit };
float_std = digit, { digit }, ".", {digit};
float_frac = ".", digit, {digit};
float_exp = ("e" | "E"), ["+" | "-"], digit, {digit};
```

## String

```ebnf
string = string_std | string_raw;
string_std = "\"", { no_double_quotation_marks | string_escape }, "\"";
string_escape = "\\", ("\"" | "\\" | "b" | "f" | "n" | "r" | "t" | ("u", unicode_hex));
string_raw = "r" string_raw_content;
string_raw_content = ("#", string_raw_content, "#") | "\"", { unicode_non_greedy }, "\"";
```

> Note: Raw strings start with an `r`, followed by n `#`s and a quotation mark
  `"`. They may contain any characters or escapes (except the end sequence).
  A raw string ends with a quotation mark (`"`), followed by n `#`s. n may be
  any number, including zero.
  Example:
  ```rust
r##"This is a "raw string". It can contain quotations or
backslashes (\)!"##
  ```
Raw strings cannot be written in EBNF, as they are context-sensitive.
Also see [the Rust document] about context-sensitivity of raw strings.

[the Rust document]: https://github.com/rust-lang/rust/blob/d046ffddc4bd50e04ffc3ff9f766e2ac71f74d50/src/grammar/raw-string-literal-ambiguity.md

## Char

```ebnf
char = "'", (no_apostrophe | "\\\\" | "\\'"), "'";
```

## Boolean

```ebnf
bool = "true" | "false";
```

## Optional

```ebnf
option = "None" | option_some;
option_some = "Some", ws, "(", ws, value, ws, ")";
```

## List

```ebnf
list = "[", [value, { comma, value }, [comma]], "]";
```

## Map

```ebnf
map = "{", [map_entry, { comma, map_entry }, [comma]], "}";
map_entry = value, ws, ":", ws, value;
```

## Tuple

```ebnf
tuple = "(", [value, { comma, value }, [comma]], ")";
```

## Struct

```ebnf
struct = unit_struct | tuple_struct | named_struct;
unit_struct = ident | "()";
tuple_struct = [ident], ws, tuple;
named_struct = [ident], ws, "(", [named_field, { comma, named_field }, [comma]], ")";
named_field = ident, ws, ":", value;
```

## Enum

```ebnf
enum_variant = enum_variant_unit | enum_variant_tuple | enum_variant_named;
enum_variant_unit = ident;
enum_variant_tuple = ident, ws, tuple;
enum_variant_named = ident, ws, "(", [named_field, { comma, named_field }, [comma]], ")";
```
