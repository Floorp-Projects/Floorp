# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.0] - 2023-06-22
### Added
- Use `empty_manifest()` to start from a manifest with no default values.
  Fixes [issue #6](https://gitlab.com/careyevans/embed-manifest/-/issues/6).
### Fixed
- Generate an object file with a single `.rsrc` section on GNU targets.
  This lets it replace the default manifest from MinGW build tools.
  Fixes [issue #5](https://gitlab.com/careyevans/embed-manifest/-/issues/5).

## [1.3.1] - 2022-08-07
### Added
- Format the code with Rustfmt.
- Assume `gnullvm` target environment should work like `gnu`.
- Add Windows 11 22H2 SDK version for maxversiontested.
### Changed
- Update `object` dependency and simplify unit tests.

## [1.3.0] - 2022-05-01
### Changed
- Use our own code again to generate COFF object files for GNU targets, but with
  better knowledge of how such files are structured, reducing dependencies and
  compile time.
- Link directly to the COFF object file instead of an archive file with one member.
### Fixed
- Make the custom `Error` type public.

## [1.2.1] - 2022-04-18
### Added
- Add checks for Windows builds to the documentation, for programs that
  should still build for non-Windows targets.

## [1.2.0] - 2022-04-17
### Added
- Generate the manifest XML from Rust code rather than requiring the developer
  to supply a correct manifest file.

## [1.1.0] - 2022-03-24
### Changed
- Use [Gimli Object](https://crates.io/crates/object) crate to build COFF
  objects containing resources for GNU targets, removing a lot of magic numbers
  and generating output more like LLVM `windres`.

## [1.0.0] - 2021-12-18
### Added
- Initial version.

[1.0.0]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.0.0
[1.1.0]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.1.0
[1.2.0]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.2.0
[1.2.1]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.2.1
[1.3.0]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.3.0
[1.3.1]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.3.1
[1.4.0]: https://gitlab.com/careyevans/embed-manifest/-/releases/v1.4.0
