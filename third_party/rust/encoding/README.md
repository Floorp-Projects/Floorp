[Encoding][doc] 0.2.33
======================

[![Encoding on Travis CI][travis-image]][travis]

[travis-image]: https://travis-ci.org/lifthrasiir/rust-encoding.png
[travis]: https://travis-ci.org/lifthrasiir/rust-encoding

Character encoding support for Rust. (also known as `rust-encoding`)
It is based on [WHATWG Encoding Standard](http://encoding.spec.whatwg.org/),
and also provides an advanced interface for error detection and recovery.

## Usage

[Complete Documentation][doc]

[doc]: https://lifthrasiir.github.io/rust-encoding/

## Usage

Put this in your `Cargo.toml`:

```toml
[dependencies]
encoding = "0.2"
```

Then put this in your crate root:

```rust
extern crate encoding;
```

## Overview

To encode a string:

~~~~ {.rust}
use encoding::{Encoding, EncoderTrap};
use encoding::all::ISO_8859_1;

assert_eq!(ISO_8859_1.encode("caf\u{e9}", EncoderTrap::Strict),
           Ok(vec![99,97,102,233]));
~~~~

To encode a string with unrepresentable characters:

~~~~ {.rust}
use encoding::{Encoding, EncoderTrap};
use encoding::all::ISO_8859_2;

assert!(ISO_8859_2.encode("Acme\u{a9}", EncoderTrap::Strict).is_err());
assert_eq!(ISO_8859_2.encode("Acme\u{a9}", EncoderTrap::Replace),
           Ok(vec![65,99,109,101,63]));
assert_eq!(ISO_8859_2.encode("Acme\u{a9}", EncoderTrap::Ignore),
           Ok(vec![65,99,109,101]));
assert_eq!(ISO_8859_2.encode("Acme\u{a9}", EncoderTrap::NcrEscape),
           Ok(vec![65,99,109,101,38,35,49,54,57,59]));
~~~~

To decode a byte sequence:

~~~~ {.rust}
use encoding::{Encoding, DecoderTrap};
use encoding::all::ISO_8859_1;

assert_eq!(ISO_8859_1.decode(&[99,97,102,233], DecoderTrap::Strict),
           Ok("caf\u{e9}".to_string()));
~~~~

To decode a byte sequence with invalid sequences:

~~~~ {.rust}
use encoding::{Encoding, DecoderTrap};
use encoding::all::ISO_8859_6;

assert!(ISO_8859_6.decode(&[65,99,109,101,169], DecoderTrap::Strict).is_err());
assert_eq!(ISO_8859_6.decode(&[65,99,109,101,169], DecoderTrap::Replace),
           Ok("Acme\u{fffd}".to_string()));
assert_eq!(ISO_8859_6.decode(&[65,99,109,101,169], DecoderTrap::Ignore),
           Ok("Acme".to_string()));
~~~~

To encode or decode the input into the already allocated buffer:

~~~~ {.rust}
use encoding::{Encoding, EncoderTrap, DecoderTrap};
use encoding::all::{ISO_8859_2, ISO_8859_6};

let mut bytes = Vec::new();
let mut chars = String::new();

assert!(ISO_8859_2.encode_to("Acme\u{a9}", EncoderTrap::Ignore, &mut bytes).is_ok());
assert!(ISO_8859_6.decode_to(&[65,99,109,101,169], DecoderTrap::Replace, &mut chars).is_ok());

assert_eq!(bytes, [65,99,109,101]);
assert_eq!(chars, "Acme\u{fffd}");
~~~~

A practical example of custom encoder traps:

~~~~ {.rust}
use encoding::{Encoding, ByteWriter, EncoderTrap, DecoderTrap};
use encoding::types::RawEncoder;
use encoding::all::ASCII;

// hexadecimal numeric character reference replacement
fn hex_ncr_escape(_encoder: &mut RawEncoder, input: &str, output: &mut ByteWriter) -> bool {
    let escapes: Vec<String> =
        input.chars().map(|ch| format!("&#x{:x};", ch as isize)).collect();
    let escapes = escapes.concat();
    output.write_bytes(escapes.as_bytes());
    true
}
static HEX_NCR_ESCAPE: EncoderTrap = EncoderTrap::Call(hex_ncr_escape);

let orig = "Hello, 世界!".to_string();
let encoded = ASCII.encode(&orig, HEX_NCR_ESCAPE).unwrap();
assert_eq!(ASCII.decode(&encoded, DecoderTrap::Strict),
           Ok("Hello, &#x4e16;&#x754c;!".to_string()));
~~~~

Getting the encoding from the string label, as specified in WHATWG Encoding standard:

~~~~ {.rust}
use encoding::{Encoding, DecoderTrap};
use encoding::label::encoding_from_whatwg_label;
use encoding::all::WINDOWS_949;

let euckr = encoding_from_whatwg_label("euc-kr").unwrap();
assert_eq!(euckr.name(), "windows-949");
assert_eq!(euckr.whatwg_name(), Some("euc-kr")); // for the sake of compatibility
let broken = &[0xbf, 0xec, 0xbf, 0xcd, 0xff, 0xbe, 0xd3];
assert_eq!(euckr.decode(broken, DecoderTrap::Replace),
           Ok("\u{c6b0}\u{c640}\u{fffd}\u{c559}".to_string()));

// corresponding Encoding native API:
assert_eq!(WINDOWS_949.decode(broken, DecoderTrap::Replace),
           Ok("\u{c6b0}\u{c640}\u{fffd}\u{c559}".to_string()));
~~~~

## Types and Stuffs

There are three main entry points to Encoding.

**`Encoding`** is a single character encoding.
It contains `encode` and `decode` methods for converting `String` to `Vec<u8>` and vice versa.
For the error handling, they receive **traps** (`EncoderTrap` and `DecoderTrap` respectively)
which replace any error with some string (e.g. `U+FFFD`) or sequence (e.g. `?`).
You can also use `EncoderTrap::Strict` and `DecoderTrap::Strict` traps to stop on an error.

There are two ways to get `Encoding`:

* `encoding::all` has static items for every supported encoding.
  You should use them when the encoding would not change or only handful of them are required.
  Combined with link-time optimization, any unused encoding would be discarded from the binary.

* `encoding::label` has functions to dynamically get an encoding from given string ("label").
  They will return a static reference to the encoding,
  which type is also known as `EncodingRef`.
  It is useful when a list of required encodings is not available in advance,
  but it will result in the larger binary and missed optimization opportunities.

**`RawEncoder`** is an experimental incremental encoder.
At each step of `raw_feed`, it receives a slice of string
and emits any encoded bytes to a generic `ByteWriter` (normally `Vec<u8>`).
It will stop at the first error if any, and would return a `CodecError` struct in that case.
The caller is responsible for calling `raw_finish` at the end of encoding process.

**`RawDecoder`** is an experimental incremental decoder.
At each step of `raw_feed`, it receives a slice of byte sequence
and emits any decoded characters to a generic `StringWriter` (normally `String`).
Otherwise it is identical to `RawEncoder`s.

One should prefer `Encoding::{encode,decode}` as a primary interface.
`RawEncoder` and `RawDecoder` is experimental and can change substantially.
See the additional documents on `encoding::types` module for more information on them.

## Supported Encodings

Encoding covers all encodings specified by WHATWG Encoding Standard and some more:

* 7-bit strict ASCII (`ascii`)
* UTF-8 (`utf-8`)
* UTF-16 in little endian (`utf-16` or `utf-16le`) and big endian (`utf-16be`)
* All single byte encoding in WHATWG Encoding Standard:
    * IBM code page 866
    * ISO 8859-{2,3,4,5,6,7,8,10,13,14,15,16}
    * KOI8-R, KOI8-U
    * MacRoman (`macintosh`), Macintosh Cyrillic encoding (`x-mac-cyrillic`)
    * Windows code pages 874, 1250, 1251, 1252 (instead of ISO 8859-1), 1253,
      1254 (instead of ISO 8859-9), 1255, 1256, 1257, 1258
* All multi byte encodings in WHATWG Encoding Standard:
    * Windows code page 949 (`euc-kr`, since the strict EUC-KR is hardly used)
    * EUC-JP and Windows code page 932 (`shift_jis`,
      since it's the most widespread extension to Shift_JIS)
    * ISO-2022-JP with asymmetric JIS X 0212 support
      (Note: this is not yet up to date to the current standard)
    * GBK
    * GB 18030
    * Big5-2003 with HKSCS-2008 extensions
* Encodings that were originally specified by WHATWG Encoding Standard:
    * HZ
* ISO 8859-1 (distinct from Windows code page 1252)

Parenthesized names refer to the encoding's primary name assigned by WHATWG Encoding Standard.

Many legacy character encodings lack the proper specification,
and even those that have a specification are highly dependent of the actual implementation.
Consequently one should be careful when picking a desired character encoding.
The only standards reliable in this regard are WHATWG Encoding Standard and
[vendor-provided mappings from the Unicode consortium](http://www.unicode.org/Public/MAPPINGS/).
Whenever in doubt, look at the source code and specifications for detailed explanations.

