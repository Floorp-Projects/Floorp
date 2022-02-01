# Change Log

All notable changes to this project will be documented in this file.
This project adheres to $[Semantic Versioning](http://semver.org/).

## [Unreleased]

## [v0.2.2] - 2021-10-09

### Fixed

- fixed `c_char` on ARM64 macOS

## [v0.2.1] - 2019-11-16

### Added

- Support for the `xtensa`, `riscv32` and `riscv64` architectures

## [v0.2.0] - 2019-02-06

### Changed

- [breaking-change] `cty::c_void` is now a type alias of `core::ffi::c_void`.

## [v0.1.5] - 2017-05-29

### Added

- More types like `int32_t`

## [v0.1.4] - 2017-05-29

### Added

- Support for the `msp430` architecture.

### Fixed

- [breaking-change] The type definitions of `c_long` and `c_ulong`.

## [v0.1.3] - 2017-05-29 - YANKED

### Added

- Support for the `nvptx` and `nvptx64` architectures.

## [v0.1.2] - 2017-05-29 - YANKED

### Fixed

- [breaking-change] The type definitions of `c_int` and `c_uint`.

## [v0.1.1] - 2017-05-29 - YANKED

### Fixed

- [breaking-change] The type definitions of `c_long`, `c_ulong` and
  `c_longlong`.

## v0.1.0 - 2017-05-24 - YANKED

- Initial release

[Unreleased]: https://github.com/japaric/cty/compare/v0.2.2...HEAD
[v0.2.2]: https://github.com/japaric/cty/compare/v0.2.1...v0.2.2
[v0.2.1]: https://github.com/japaric/cty/compare/v0.2.0...v0.2.1
[v0.2.0]: https://github.com/japaric/cty/compare/v0.1.5...v0.2.0
[v0.1.5]: https://github.com/japaric/cty/compare/v0.1.4...v0.1.5
[v0.1.4]: https://github.com/japaric/cty/compare/v0.1.3...v0.1.4
[v0.1.3]: https://github.com/japaric/cty/compare/v0.1.2...v0.1.3
[v0.1.2]: https://github.com/japaric/cty/compare/v0.1.1...v0.1.2
[v0.1.1]: https://github.com/japaric/cty/compare/v0.1.0...v0.1.1
