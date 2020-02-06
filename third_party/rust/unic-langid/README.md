# unic-langid [![Build Status](https://travis-ci.org/zbraniecki/unic-locale.svg?branch=master)](https://travis-ci.org/zbraniecki/unic-locale) [![Coverage Status](https://coveralls.io/repos/github/zbraniecki/unic-locale/badge.svg?branch=master)](https://coveralls.io/github/zbraniecki/unic-locale?branch=master)

`unic-langid` is an API for managing [Unicode Language Identifiers](http://unicode.org/reports/tr35/#Unicode_language_identifier).

The crate provides a way to create a struct from a string, manipulate its fields, canonicalize it, and serialize into a string.

Usage
-----

```rust
use unic_langid::LanguageIdentifier;

let loc: LanguageIdentifier = "en-US".parse().expect("Parsing failed.");

assert_eq!(loc.language(), "en");
assert_eq!(loc.script(), None);
assert_eq!(loc.region(), Some("US"));

loc.set_script(Some("latn"));

assert_eq!(&loc.to_string(), "en-Latn-US");
```

```rust
use unic_langid::LanguageIdentifier;

let langid = LanguageIdentifier::from_parts(
    Some("en"),
    None,
    None,
    Some(&["nedis", "macos"])
).expect("Parsing failed.");

assert_eq!(&langid.to_string(), "en-macos-nedis")
```

Macros
------

`unic-langid` can be also compiled with `features = ["macros"]` which enables `langid!` macro:

```rust
use unic_langid::langid;

// The `const` assignment will currently work only
// if the langid doesn't contain any variants.
const EN_US: LanguageIdentifier = langid!("en-US");

fn main() {
    let de_at = langid!("de-AT");

    assert_eq!(&de_at.to_string(), "de-AT")
    assert_eq!(&EN_US.to_string(), "en-US")
}
```

The macro allows for compile-time parsing and validation of literal language identifiers.

Status
------

The crate is mostly complete on the `well-formed` level. The next batch of work will go into
API maturity.

Get Involved
------------

`unic-langid` is open-source, licensed under the Apache License, Version 2.0.  We
encourage everyone to take a look at our code and we'll listen to your
feedback.
