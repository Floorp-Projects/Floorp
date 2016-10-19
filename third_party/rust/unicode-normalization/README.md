Unicode character composition and decomposition utilities
as described in
[Unicode Standard Annex #15](http://www.unicode.org/reports/tr15/).

[![Build Status](https://travis-ci.org/unicode-rs/unicode-normalization.svg)](https://travis-ci.org/unicode-rs/unicode-normalization)

[Documentation](https://unicode-rs.github.io/unicode-normalization/unicode_normalization/index.html)

```rust
extern crate unicode_normalization;

use unicode_normalization::char::compose;
use unicode_normalization::UnicodeNormalization;

fn main() {
    assert_eq!(compose('A','\u{30a}'), Some('Å'));
    
    let s = "ÅΩ";
    let c = s.nfc().collect::<String>();
    assert_eq!(c, "ÅΩ");
}
```

# crates.io

You can use this package in your project by adding the following
to your `Cargo.toml`:

```toml
[dependencies]
unicode-normalization = "0.1.1"
```
