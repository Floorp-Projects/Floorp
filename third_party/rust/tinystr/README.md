# tinystr [![crates.io](http://meritbadge.herokuapp.com/tinystr)](https://crates.io/crates/tinystr) [![Build Status](https://travis-ci.org/zbraniecki/tinystr.svg?branch=master)](https://travis-ci.org/zbraniecki/tinystr) [![Coverage Status](https://coveralls.io/repos/github/zbraniecki/tinystr/badge.svg?branch=master)](https://coveralls.io/github/zbraniecki/tinystr?branch=master)

`tinystr` is a small ASCII-only bounded length string representation.

Usage
-----

```rust
use tinystr::{TinyStr4, TinyStr8, TinyStr16};

fn main() {
    let s1: TinyStr4 = "tEsT".parse()
        .expect("Failed to parse.");

    assert_eq!(s1, "tEsT");
    assert_eq!(s1.to_ascii_uppercase(), "TEST");
    assert_eq!(s1.to_ascii_lowercase(), "test");
    assert_eq!(s1.to_ascii_titlecase(), "Test");
    assert_eq!(s1.is_ascii_alphanumeric(), true);

    let s2: TinyStr8 = "New York".parse()
        .expect("Failed to parse.");

    assert_eq!(s2, "New York");
    assert_eq!(s2.to_ascii_uppercase(), "NEW YORK");
    assert_eq!(s2.to_ascii_lowercase(), "new york");
    assert_eq!(s2.to_ascii_titlecase(), "New york");
    assert_eq!(s2.is_ascii_alphanumeric(), false);

    let s3: TinyStr16 = "metaMoRphosis123".parse()
        .expect("Failed to parse.");

    assert_eq!(s3, "metaMoRphosis123");
    assert_eq!(s3.to_ascii_uppercase(), "METAMORPHOSIS123");
    assert_eq!(s3.to_ascii_lowercase(), "metamorphosis123");
    assert_eq!(s3.to_ascii_titlecase(), "Metamorphosis123");
    assert_eq!(s3.is_ascii_alphanumeric(), true);
}
```

Details
-------

The crateh provides three structs:
 * `TinyStr4` an ASCII-only string limited to 4 characters.
 * `TinyStr8` an ASCII-only string limited to 8 characters.
 * `TinyStr16` an ASCII-only string limited to 16 characters.

It stores them as `u32`/`u64`/`u128` and uses bitmasking to provide basic string manipulation operations:
 * is_ascii_numeric
 * is_ascii_alphabetic
 * is_ascii_alphanumeric
 * to_ascii_lowercase
 * to_ascii_uppercase
 * to_ascii_titlecase
 * PartialEq

This set is sufficient for certain classes of uses such as `unic-langid` libraries.

Performance
-----------

For those uses, TinyStr provides [performance characteristics](https://github.com/zbraniecki/tinystr/wiki/Performance) much better than the regular `String`.

Status
------

The crate is fully functional and ready to be used in production.
The capabilities can be extended.

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
</sub>
