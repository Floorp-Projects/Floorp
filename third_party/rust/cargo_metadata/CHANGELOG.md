# Changelog

## Unreleased

### Added

- Re-exported `semver` crate directly.

### Changed

- Made `parse_stream` more versatile by accepting anything that implements `Read`.

### Removed

- Removed re-exports for `BuildMetadata` and `Prerelease` from `semver` crate.

### Fixed

- Added missing `manifest_path` field to `Artifact`. Fixes #187.

## [0.15.0] - 2022-06-22

### Added

- Re-exported `BuildMetadata` and `Prerelease` from `semver` crate.
- Added `workspace_packages` function.
- Added `Edition` enum to better parse edition field.
- Added `rust-version` field to Cargo manifest.

### Changed

- Bumped msrv from `1.40.0` to `1.42.0`.

### Internal Changes

- Updated `derive_builder` to the latest version.
- Made use of `matches!` macros where possible.
- Fixed some tests
