# Fluent LangNeg

**Fluent LangNeg is a library for language and locale identifier negotiation.**

[![crates.io](http://meritbadge.herokuapp.com/fluent-langneg)](https://crates.io/crates/fluent-langneg)
[![Build Status](https://travis-ci.org/projectfluent/fluent-langneg-rs.svg?branch=master)](https://travis-ci.org/projectfluent/fluent-langneg-rs)
[![Coverage Status](https://coveralls.io/repos/github/projectfluent/fluent-langneg-rs/badge.svg?branch=master)](https://coveralls.io/github/projectfluent/fluent-langneg-rs?branch=master)

Introduction
------------

This is a Rust implementation of fluent-langneg library which is a part of Project Fluent.

The library uses [unic-langid](https://github.com/zbraniecki/unic-locale) and [unic-locale](https://github.com/zbraniecki/unic-locale) to retrieve and operate on Unicode Language and Locale Identifiers.
The library provides algorithm for negotiating between lists of locales.

Usage
-----

```rust
use fluent_langneg::negotiate_languages;
use fluent_langneg::NegotiationStrategy;
use fluent_langneg::convert_vec_str_to_langids_lossy;
use unic_langid::LanguageIdentifier

// Since langid parsing from string is fallible, we'll use a helper
// function which strips any langids that failed to parse.
let requested = convert_vec_str_to_langids_lossy(&["de-DE", "fr-FR", "en-US"]);
let available = convert_vec_str_to_langids_lossy(&["it", "fr", "de-AT", "fr-CA", "en-US"]);
let default: LanguageIdentifier = "en-US".parse().expect("Parsing langid failed.");

let supported = negotiate_languages(
  &requested,
  &available,
  Some(&default),
  NegotiationStrategy::Filtering
);

let expected = convert_vec_str_to_langids_lossy(&["de-AT", "fr", "fr-CA", "en-US"]);
assert_eq!(supported,
            expected.iter().map(|t| t.as_ref()).collect::<Vec<&LanguageIdentifier>>());
```

See [docs.rs][] for more examples.

[docs.rs]: https://docs.rs/fluent-langneg/

Status
------

The implementation is complete according to fluent-langneg
corpus of tests, which means that it parses, serializes and negotiates as expected.

The negotiation methods can operate on lists of `LanguageIdentifier` or `Locale`.

The remaining work is on the path to 1.0 is to gain in-field experience of using it,
add more tests and ensure that bad input is correctly handled.

Compatibility
-------------

The API is based on [UTS 35][] definition of [Unicode Locale Identifier][] and is aiming to
parse and serialize all locale identifiers according to that definition.

*Note*: Unicode Locale Identifier is similar, but different, from what [BCP47][] specifies under
the name Language Tag.
For most locale management and negotiation needs, the Unicode Locale Identifier used in this crate is likely a better choice,
but in some case, like HTTP Accepted Headers, you may need the complete BCP47 Language Tag implementation which
this crate does not provide.

Language negotiation algorithms are custom Project Fluent solutions,
based on [RFC4647][].

The language negotiation strategies aim to replicate the best-effort matches with
the most limited amount of data. The algorithm returns reasonable
results without any database, but the results can be improved with either limited
or full [CLDR likely-subtags][] database.

The result is a balance chosen for Project Fluent and may differ from other
implementations of language negotiation algorithms which may choose different
tradeoffs.

[BCP47]: https://tools.ietf.org/html/bcp47
[RFC6067]: https://www.ietf.org/rfc/rfc6067.txt
[UTS 35]: http://www.unicode.org/reports/tr35/#Locale_Extension_Key_and_Type_Data
[RFC4647]: https://tools.ietf.org/html/rfc4647
[CLDR likely-subtags]: http://www.unicode.org/cldr/charts/latest/supplemental/likely_subtags.html
[Unicode Locale Identifier]: (http://unicode.org/reports/tr35/#Identifiers)

Alternatives
------------

Although Fluent Locale aims to stay close to W3C Accepted Languages, it does not aim
to implement the full behavior and some aspects of the language negotiation strategy
recommended by W3C, such as weights, are not a target right now.

For such purposes, [rust-language-tags][] crate seems to be a better choice.

[rust-language-tags]: https://github.com/pyfisch/rust-language-tags

Performance
-----------

The crate is considered to be fully optimized for production.


Develop
-------

    cargo build
    cargo test
    cargo bench

