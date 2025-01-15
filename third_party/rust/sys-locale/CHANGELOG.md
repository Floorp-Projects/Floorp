# sys-locale changelog

Notable changes to this project will be documented in the [keep a changelog](https://keepachangelog.com/en/1.0.0/) format.

## [Unreleased]

## [0.3.1] - 2023-08-27

### Added
- Added support for getting a list of user locales in their preferred order via `get_locales`.
  - Additional locales are currently supported on iOS, macOS, WASM, and Windows. Other platforms will
  only return a single locale like `get_locale` does.

### Changed
- Removed `windows-sys` dependency

## [0.3.0] - 2023-04-04

### Changed
- The crate now only uses `wasm-bindgen` when targeting WebAssembly on the web.
  Use the new `js` feature to target the web.

### Fixed
- The crate now compiles for unsupported platforms.
- Cleaned up typos and grammar in README.

# [0.2.4] - 2023-03-07

### Changed
- Removed dependency on the `winapi` crate in favor of `windows-sys`, following more of the wider ecosystem.

## [0.2.3] - 2022-11-06

### Fixed
- Re-release 0.2.2 and correctly maintain `no_std` compatibility on Apple targets.

## [0.2.2] - 2022-11-06

### Changed
- The Apple backend has been rewritten in pure Rust instead of Objective-C.

### Fixed
- The locale returned on UNIX systems is now always a correctly formatted BCP-47 tag.

## [0.2.1] - 2022-06-16

### Added

- The crate now supports being used via WASM in a WebWorker environment.

## [0.2.0] - 2022-03-01

### Fixed

- Fixed a soundness issue on Linux and BSDs by querying the environment directly instead of using libc setlocale. The libc setlocale is not safe for use in a multi-threaded context.

### Changed

- No longer `no_std` on Linux and BSDs

## [0.1.0] - 2021-05-13

Initial release

[Unreleased]: https://github.com/1Password/sys-locale/compare/v0.3.1...HEAD
[0.1.0]: https://github.com/1Password/sys-locale/releases/tag/v0.1.0
[0.2.0]: https://github.com/1Password/sys-locale/releases/tag/v0.2.0
[0.2.1]: https://github.com/1Password/sys-locale/releases/tag/v0.2.1
[0.2.2]: https://github.com/1Password/sys-locale/releases/tag/v0.2.2
[0.2.3]: https://github.com/1Password/sys-locale/releases/tag/v0.2.3
[0.2.4]: https://github.com/1Password/sys-locale/releases/tag/v0.2.4
[0.3.0]: https://github.com/1Password/sys-locale/releases/tag/v0.3.0
[0.3.1]: https://github.com/1Password/sys-locale/releases/tag/v0.3.1
