Iterators which split strings on Grapheme Cluster or Word boundaries, according
to the [Unicode Standard Annex #29](http://www.unicode.org/reports/tr29/) rules.

[![Build Status](https://travis-ci.org/unicode-rs/unicode-segmentation.svg)](https://travis-ci.org/unicode-rs/unicode-segmentation)

[Documentation](https://unicode-rs.github.io/unicode-segmentation/unicode_segmentation/index.html)

```rust
extern crate unicode_segmentation;

use unicode_segmentation::UnicodeSegmentation;

fn main() {
    let s = "a̐éö̲\r\n";
    let g = UnicodeSegmentation::graphemes(s, true).collect::<Vec<&str>>();
    let b: &[_] = &["a̐", "é", "ö̲", "\r\n"];
    assert_eq!(g, b);

    let s = "The quick (\"brown\") fox can't jump 32.3 feet, right?";
    let w = s.unicode_words().collect::<Vec<&str>>();
    let b: &[_] = &["The", "quick", "brown", "fox", "can't", "jump", "32.3", "feet", "right"];
    assert_eq!(w, b);

    let s = "The quick (\"brown\")  fox";
    let w = s.split_word_bounds().collect::<Vec<&str>>();
    let b: &[_] = &["The", " ", "quick", " ", "(", "\"", "brown", "\"", ")", " ", " ", "fox"];
    assert_eq!(w, b);
}
```

# no_std

unicode-segmentation does not depend on libstd, so it can be used in crates
with the `#![no_std]` attribute.

# crates.io

You can use this package in your project by adding the following
to your `Cargo.toml`:

```toml
[dependencies]
unicode-segmentation = "1.1.0"
```

# Change Log

## 1.2.0

* New `GraphemeCursor` API allows random access and bidirectional iteration.
* Fixed incorrect splitting of certain emoji modifier sequences.

## 1.1.0

* Add `as_str` methods to the iterator types.

## 1.0.3

* Code cleanup and additional tests.

## 1.0.1

* Fix a bug affecting some grapheme clusters containing Prepend characters.

## 1.0.0

* Upgrade to Unicode 9.0.0.
