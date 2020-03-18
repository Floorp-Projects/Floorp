# unicode-width

Determine displayed width of `char` and `str` types according to
[Unicode Standard Annex #11](http://www.unicode.org/reports/tr11/)
rules.

[![Build Status](https://travis-ci.org/unicode-rs/unicode-width.svg)](https://travis-ci.org/unicode-rs/unicode-width)

[Documentation](https://unicode-rs.github.io/unicode-width/unicode_width/index.html)

```rust
extern crate unicode_width;

use unicode_width::UnicodeWidthStr;

fn main() {
    let teststr = "Ｈｅｌｌｏ, ｗｏｒｌｄ!";
    let width = UnicodeWidthStr::width(teststr);
    println!("{}", teststr);
    println!("The above string is {} columns wide.", width);
    let width = teststr.width_cjk();
    println!("The above string is {} columns wide (CJK).", width);
}
```

## features

unicode-width does not depend on libstd, so it can be used in crates
with the `#![no_std]` attribute.

## crates.io

You can use this package in your project by adding the following
to your `Cargo.toml`:

```toml
[dependencies]
unicode-width = "0.1.4"
```
