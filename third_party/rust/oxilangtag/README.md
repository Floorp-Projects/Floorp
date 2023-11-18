oxilangtag
==========

[![actions status](https://github.com/oxigraph/oxilangtag/workflows/build/badge.svg)](https://github.com/oxigraph/oxilangtag/actions)
[![Latest Version](https://img.shields.io/crates/v/oxilangtag.svg)](https://crates.io/crates/oxilangtag)
[![Released API docs](https://docs.rs/oxilangtag/badge.svg)](https://docs.rs/oxilangtag)

OxiLangTag is a Rust library allowing to validate and normalize language tags following [RFC 5646](https://tools.ietf.org/html/rfc5646)
([BCP 47](https://tools.ietf.org/html/bcp47)).

It is a fork of the [`language-tags`](https://github.com/pyfisch/rust-language-tags/) focusing on [RDF use cases](https://www.w3.org/TR/rdf11-primer/).
You might find the [`language-tags`](https://github.com/pyfisch/rust-language-tags/) crate more convenient.

It allows zero stack allocation language tag validation.
Getters are also provided to easily retrieve the various language tag components.

If [`serde`](https://serde.rs/) is available, `LanguageTag` implements the `Serialize` and `Deserialize` traits and encodes the language tag as a string.

Example:
```rust
use oxilangtag::LanguageTag;

// Parsing and validation
let language_tag = LanguageTag::parse("zh-cmn-Hans-CN-x-test").unwrap();
assert_eq!(language_tag.as_str(), "zh-cmn-Hans-CN-x-test");

// Language tag components
assert_eq!(language_tag.primary_language(), "zh");
assert_eq!(language_tag.extended_language(), Some("cmn"));
assert_eq!(language_tag.full_language(), "zh-cmn");
assert_eq!(language_tag.script(), Some("Hans"));
assert_eq!(language_tag.region(), Some("CN"));
assert_eq!(language_tag.extension(), None);
assert_eq!(language_tag.private_use_subtags().collect::<Vec<_>>(), vec!["test"]);
```

## License

This project is licensed under the MIT license ([LICENSE-MIT](LICENSE-MIT) or `<http://opensource.org/licenses/MIT>`).

It is based on the [`language-tags`](https://github.com/pyfisch/rust-language-tags/) crate by [pyfisch](https://github.com/pyfisch) under MIT license.


### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in Oxilangtag by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.
