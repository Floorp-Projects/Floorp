# Changelog

## [Unreleased]

## [0.3.0] - 2019-08-08
### Changed
- Unsafe code is forbidden now

### Fixed
- Fixed panic from slicing unicode character in image attr braces

### Added
- Added support for FQDN addresses
- Added support for parsing ice-pacing
- Added fuzzing target

## [0.2.2] - 2019-06-21
### Changed
 - Minimum Rust version >= 1.35

## [0.2.0] - 2019-06-15
### Changed
- Minimum Rust version >= 1.30.0
- Changed code coverage from kcov to tarpaulin
- Moved file parser example to examples sub directory
- Replaced cause() with source() in unit test
- Moved all unit tests into tests modules

### Fixed
- Unknown extensions in candidate attributes (#103)
- Reduced amount of internal clone() calls significantly
- Added dyn to error:Error impl required by more recent rust versions

### Added
- Support for anonymization to enable logging of SDP without personal
  information
- Quite a bit more unit testing got added

### Removed
- Replaced unsupported types with errors directly in lib.rs

## [0.1.0] - 2019-01-26
- Initial release
- Minimum Rust version >= 1.17.0
