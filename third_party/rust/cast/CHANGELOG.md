# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

## [v0.2.2] - 2017-05-07

### Fixed

- UB in the checked cast from `f32` to `u128`.

## [v0.2.1] - 2017-05-06

### Added

- Support for 128-bit integers, behind the `x128` Cargo feature (nightly
  needed).

## [v0.2.0] - 2017-02-08

### Added

- Now `cast::Error` implements the `std::error::Error` trait among other traits
  like `Display`, `Clone`, etc.

### Changed

- [breaking-change] This crate now depends on the `std` crate by default but you
  can make it `no_std` by opting out of the `std` Cargo feature.

## v0.1.0 - 2016-02-07

Initial release

[Unreleased]: https://github.com/japaric/cast.rs/compare/v0.2.2...HEAD
[v0.2.2]: https://github.com/japaric/cast.rs/compare/v0.2.1...v0.2.2
[v0.2.1]: https://github.com/japaric/cast.rs/compare/v0.2.0...v0.2.1
[v0.2.0]: https://github.com/japaric/cast.rs/compare/v0.1.0...v0.2.0
