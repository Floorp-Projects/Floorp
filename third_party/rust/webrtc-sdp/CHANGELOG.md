# Changelog
## [0.3.4] - 2020-03-31
### Fixed
- Fixed new clippy warnings in stable
- Accept a lack of c= lines if there are no m= lines (for JSEP compat.)
### Changed
- Added support for ssrc-group
- Added support for RTX FMTP parameters
- Example runner can no be told to expect failure
## [0.3.3] - 2019-12-10
### Changed
- Changed handling of default channel counts

## [0.3.2] - 2019-12-02
### Changed
- Fixed handling of spaces in fmtp attributes
- Minimum Rust version >= 1.36

## [0.3.1] - 2019-09-12
### Changed
- Updated `urls` dependency to `0.2.1`

### Removed
- Removed `TcpTlsRtpSavpf` protocl token
- Removed dependency on `enum-display-derive`

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
