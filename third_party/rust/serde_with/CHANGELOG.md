# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

* Add a helper to deserialize a `Vec<u8>` from `String` (#35)
* Add `default_on_error` helper, which turns errors into `Default`s of the type
* Add `default_on_null` helper, which turns `null` values into `Default`s of the type

### Changed

* Bump minimal Rust version to 1.36.0
    * Support Rust Edition 2018
    * version-sync depends on smallvec which requires 1.36
* Improved CI pipeline by running `cargo audit` and `tarpaulin` in all configurations now.

## [1.3.1]

### Fixed

* Use `serde_with_macros` with proper dependencies specified.

## [1.3.0]

### Added

* Add `skip_serializing_none` attribute, which adds `#[serde(skip_serializing_if = "Option::is_none")]` for each Option in a struct.
    This is helpfull for APIs which have many optional fields.
    The effect of can be negated by adding `serialize_always` on those fields, which should always be serialized.
    Existing `skip_serializing_if` will never be modified and those fields keep their behavior.

## [1.2.0]

### Added

* Add macro helper to support deserializing values with nested or flattened syntax #38
* Serialize tuple list as map helper

### Changed

* Bumped minimal Rust version to 1.30.0

## [1.1.0]

### Added

* Serialize HashMap/BTreeMap as list of tuples

## [1.0.0]

### Added

* No changes in this release.
* Bumped version number to indicate the stability of the library.

## [0.2.5]

### Added

* Helper which deserializes an empty string as `None` and otherwise uses `FromStr` and `AsRef<str>`.

## [0.2.4]

### Added

* De/Serialize sequences by using `Display` and `FromStr` implementations on each element. Contributed by @katyo

## [0.2.3]

### Added

* Add missing docs and enable deny missing_docs
* Add badges to Cargo.toml and crates.io

### Changed

* Improve Travis configuration
* Various clippy improvements

## [0.2.2]

### Added

* `unwrap_or_skip` allows to transparently serialize the inner part of a `Some(T)`
* Add deserialization helpser for sets and maps, inspired by [comment](https://github.com/serde-rs/serde/issues/553#issuecomment-299711855)
    * Create an error if duplicate values for a set are detected
    * Create an error if duplicate keys for a map are detected
    * Implement a first-value wins strategy for sets/maps. This is different to serde's default
        which implements a last value wins strategy.

## [0.2.1]

### Added

* Double Option pattern to differentiate between missing, unset, or existing value
* `with_prefix!` macro, which puts a prefix on every struct field

## [0.2.0]

### Added

* Add chrono support: Deserialize timestamps from int, float, and string
* Serialization of embedded JSON strings
* De/Serialization using `Display` and `FromStr` implementations
* String-based collections using `Display` and `FromStr`, allows to deserialize "#foo,#bar"

## [0.1.0]

### Added

* Reserve name on crates.io
