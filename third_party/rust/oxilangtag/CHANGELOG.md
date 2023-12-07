# Changelog

## [0.1.3] - 2022-03-26

### Added
- `LanguageTag` now implements Serde `Serialize` and `Deserialize` trait if the `serde` crate is present. 
 The serialization is a plain string.


## [0.1.2] - 2021-04-16

### Added
- `LanguageTag` struct with a parser, case normalization and components accessors.

### Changed
- Proper attribution from [`language-tags`](https://github.com/pyfisch/rust-language-tags/).
