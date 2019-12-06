**DEPRECATED:** This crate has been folded into the
[`regex-syntax`](https://docs.rs/regex-syntax) and is now deprecated.

utf8-ranges
===========
This crate converts contiguous ranges of Unicode scalar values to UTF-8 byte
ranges. This is useful when constructing byte based automata from Unicode.
Stated differently, this lets one embed UTF-8 decoding as part of one's
automaton.

[![Linux build status](https://api.travis-ci.org/BurntSushi/utf8-ranges.png)](https://travis-ci.org/BurntSushi/utf8-ranges)
[![](http://meritbadge.herokuapp.com/utf8-ranges)](https://crates.io/crates/utf8-ranges)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Documentation

https://docs.rs/utf8-ranges


### Example

This shows how to convert a scalar value range (e.g., the basic multilingual
plane) to a sequence of byte based character classes.


```rust
extern crate utf8_ranges;

use utf8_ranges::Utf8Sequences;

fn main() {
    for range in Utf8Sequences::new('\u{0}', '\u{FFFF}') {
        println!("{:?}", range);
    }
}
```

The output:

```text
[0-7F]
[C2-DF][80-BF]
[E0][A0-BF][80-BF]
[E1-EC][80-BF][80-BF]
[ED][80-9F][80-BF]
[EE-EF][80-BF][80-BF]
```

These ranges can then be used to build an automaton. Namely:

1. Every arbitrary sequence of bytes matches exactly one of the sequences of
   ranges or none of them.
2. Every match sequence of bytes is guaranteed to be valid UTF-8. (Erroneous
   encodings of surrogate codepoints in UTF-8 cannot match any of the byte
   ranges above.)
