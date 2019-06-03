# shift_or_euc

[![Apache 2 / MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/shift_or_euc/blob/master/COPYRIGHT)

A Japanese legacy encoding detector for detecting between Shift_JIS, EUC-JP,
and, optionally, ISO-2022-JP _given_ the assumption that the encoding is one
of those.

This detector is generally more accurate (but see below about the failure
mode on half-width katakana) and decides much sooner than machine
learning-based detectors. To decide EUC-JP, machine learning-based detectors
try to gain confidence that the input looks like EUC-JP. To decide EUC-JP,
this detector instead looks for two simple rule-based signs of the input not
being Shift_JIS.

As a consequence of not containing machine learning tables, the binary size
footprint that this crate adds on top of
[`encoding_rs`](https://docs.rs/crate/encoding_rs) is tiny.

## Documentation

[API documentation on docs.rs](https://docs.rs/crate/shift_or_euc)

## Licensing

See the file named [COPYRIGHT](https://github.com/hsivonen/shift_or_euc/blob/master/COPYRIGHT).

## Sample Program Usage

1. [Install Rust](https://rustup.rs/)
2. `git clone https://github.com/hsivonen/shift_or_euc`
3. `cd shift_or_euc`
4. `cargo run --example detect PATH_TO_FILE`

The program prints one of:

* Shift_JIS
* EUC-JP
* ISO-2022-JP
* Undecided

## Principle of Operation

The detector is based on two observations:

1. The ISO-2022-JP escape sequences don't normally occur in Shift_JIS or
EUC-JP, so encountering such an escape sequence (before non-ASCII has been
encountered) can be taken as indication of ISO-2022-JP.
2. When normal (full-with) kana or common kanji encoded as Shift_JIS is
decoded as EUC-JP, or vice versa, the result is either an error or half-width
katakana, and it's very uncommon for Japanese HTML to have half-width katakana
character before a normal kana or common kanji character. Therefore, if
decoding as Shift_JIS results in error or have-width katakana, the detector
decides that the content is EUC-JP, and vice versa.

## Failure Modes

The detector gives the wrong answer if the text has a half-width katakana
character before normal kana or common kanji. Some uncommon kanji are
undecidable. (All JIS X 0208 Level 1 kanji are decidable.)

The half-width katakana issue is mainly relevant for old 8-bit JIS X 0201-only
text files that would decode correctly as Shift_JIS but that the detector
detects as EUC-JP.

The undecidable kanji issue does not realistically show up when a full
document is fed to the detector, because, realistically, in a full document,
there is at least one kana or common kanji. It can occur, though, if the
detector is only run on a prefix of a document and the prefix only contains
the title of the document. It is possible for document title to consist
entirely of undecidable kanji. (Indeed, Japanese Wikipedia has articles with
such titles.) If the detector is undecided, falling back to Shift_JIS is
typically the Web oriented better guess.