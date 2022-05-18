# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [1.6.4]

### Fixed

* Fix compiling when having a struct field without the `serde_as` annotation by updating `serde_with_macros`.
    This broke in 1.4.0 of `serde_with_macros`. [#267](https://github.com/jonasbb/serde_with/issues/267)

## [1.6.3]

### Changed

* Bump macro crate dependency (`serde_with_macros`) to 1.4.0 to pull in those improvements.

## [1.6.2]

### Added

* New function `serde_with::rust::deserialize_ignore_any`.
    This function allows deserializing any data and returns the default value of the type.
    This can be used in conjunction with `#[serde(other)]` to allow deserialization of unknown data carrying enum variants.

    Thanks to @lovasoa for suggesting and implementing it.

## [1.6.1]

### Added

* Add new types similar to `DurationSeconds` and `TimestampSeconds` but for base units of milliseconds, microseconds, and nanoseconds.
    The `*WithFrac` variants also exist.
* Add `SerializeAs` implementation for references.

### Changed

* Release `Sized` trait bound from `As`, `Same`, `SerializeAs`, and `SerializeAsWrap`.
    Only the serialize part is relaxed.

## [1.6.0]

### Added

* Add `DefaultOnNull` as the equivalent for `rust::default_on_null` but for the `serde_as` system.
* Support specifying a path to the `serde_with` crate for the `serde_as` and derive macros.
    This is useful when using crate renaming in Cargo.toml or while re-exporting the macros.

    Many thanks to @tobz1000 for raising the issue and contributing fixes.

### Changed

* Bump minimum supported rust version to 1.40.0

## [1.5.1]

### Fixed

* Depend on serde with the `derive` feature enabled.
    The `derive` feature is required to deserliaze untagged enums which are used in the `DefaultOnError` helpers.
    This fixes compilation of `serde_with` in scenarios where no other crate enables the `derive` feature.

## [1.5.0]

### Added

* The largest addition to this release is the addition of the `serde_as` de/serialization scheme.
    It's goal is it to be a more flexible replacement to serde's with-annotation, by being more composable than before.
    No longer is it a problem to add a custom de/serialization adapter is the type is within an `Option` or a `Vec`.

    Thanks to `@markazmierczak` for the design of the trait without whom this wouldn't be possible.

    More details about this new scheme can be found in the also new [user guide](https://docs.rs/serde_with/1.5.0/serde_with/guide/index.html)
* This release also features a detailed user guide.
    The guide focusses more on how to use this crate by providing examples.
    For example, it includes a section about the available feature flags of this crate and how you can migrate to the shiny new `serde_as` scheme.
* The crate now features de/serialization adaptors for the std and chrono's `Duration` types. #56 #104
* Add a `hex` module, which allows formatting bytes (i.e. `Vec<u8>`) as a hexadecimal string.
    The formatting supports different arguments how the formatting is happening.
* Add two derive macros, `SerializeDisplay` and `DeserializeFromStr`, which implement the `Serialize`/`Deserialize` traits based on `Display` and `FromStr`.
    This is in addition to the already existing methods like `DisplayFromStr`, which act locally, whereas the derive macros provide the traits expected by the rest of the ecosystem.

    This is part of `serde_with_macros` v1.2.0.
* Added some `serialize` functions to modules which previously had none.
    This makes it easier to use the conversion when also deriving `Serialialize`.
    The functions simply pass through to the underlying `Serialize` implementation.
    This affects `sets_duplicate_value_is_error`, `maps_duplicate_key_is_error`, `maps_first_key_wins`, `default_on_error`, and `default_on_null`.
* Added `sets_last_value_wins` as a replacement for `sets_first_value_wins` which is deprecated now.
    The default behavior of serde is to prefer the first value of a set so the opposite is taking the last value.
* Added `#[serde_as]` compatible conversion methods for serializing durations and timestamps as numbers.
    The four types `DurationSeconds`, `DurationSecondsWithFrac`, `TimestampSeconds`, `TimestampSecondsWithFrac` provide the serialization conversion with optional subsecond precision.
    There is support for `std::time::Duration`, `chrono::Duration`, `std::time::SystemTime` and `chrono::DateTime`.
    Timestamps are serialized as a duration since the UNIX epoch.
    The serialization can be customized.
    It supports multiple formats, such as `i64`, `f64`, or `String`, and the deserialization can be tweaked if it should be strict or lenient when accepting formats.

### Changed

* Convert the code to use 2018 edition.
* @peterjoel improved the performance of `with_prefix!`. #101

### Fixed

* The `with_prefix!` macro, to add a string prefixes during serialization, now also works with unit variant enum types. #115 #116
* The `serde_as` macro now supports serde attributes and no longer panic on unrecognized values in the attribute.
    This is part of `serde_with_macros` v1.2.0.

### Deprecated

* Deprecate `sets_first_value_wins`.
    The default behavior of serde is to take the first value, so this module is not necessary.

## [1.5.0-alpha.2]

### Added

* Add a `hex` module, which allows formatting bytes (i.e. `Vec<u8>`) as a hexadecimal string.
    The formatting supports different arguments how the formatting is happening.
* Add two derive macros, `SerializeDisplay` and `DeserializeFromStr`, which implement the `Serialize`/`Deserialize` traits based on `Display` and `FromStr`.
    This is in addition to the already existing methods like `DisplayFromStr`, which act locally, whereas the derive macros provide the traits expected by the rest of the ecosystem.

    This is part of `serde_with_macros` v1.2.0-alpha.3.

### Fixed

* The `serde_as` macro now supports serde attributes and no longer panic on unrecognized values in the attribute.
    This is part of `serde_with_macros` v1.2.0-alpha.2.

## [1.5.0-alpha.1]

### Added

* The largest addition to this release is the addition of the `serde_as` de/serialization scheme.
    It's goal is it to be a more flexible replacement to serde's with-annotation, by being more composable than before.
    No longer is it a problem to add a custom de/serialization adapter is the type is within an `Option` or a `Vec`.

    Thanks to `@markazmierczak` for the design of the trait without whom this wouldn't be possible.

    More details about this new scheme can be found in the also new [user guide](https://docs.rs/serde_with/1.5.0-alpha.1/serde_with/guide/index.html)
* This release also features a detailed user guide.
    The guide focusses more on how to use this crate by providing examples.
    For example, it includes a section about the available feature flags of this crate and how you can migrate to the shiny new `serde_as` scheme.
* The crate now features de/serialization adaptors for the std and chrono's `Duration` types. #56 #104

### Changed

* Convert the code to use 2018 edition.
* @peterjoel improved the performance of `with_prefix!`. #101

### Fixed

* The `with_prefix!` macro, to add a string prefixes during serialization, now also works with unit variant enum types. #115 #116

## [1.4.0]

### Added

* Add a helper to deserialize a `Vec<u8>` from `String` (#35)
* Add `default_on_error` helper, which turns errors into `Default`s of the type
* Add `default_on_null` helper, which turns `null` values into `Default`s of the type

### Changed

* Bump minimal Rust version to 1.36.0
    * Supports Rust Edition 2018
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
