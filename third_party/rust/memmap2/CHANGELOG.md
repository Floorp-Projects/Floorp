# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

## [0.3.1] - 2021-08-15
### Fixed
- Integer overflow during file length calculation on 32bit targets.
- Stub implementation. [@Mrmaxmeier](https://github.com/Mrmaxmeier)

## [0.3.0] - 2021-06-10
### Changed
- `MmapOptions` allows mapping using Unix descriptors and not only `std::fs::File` now.
  [@mripard](https://github.com/mripard)

## [0.2.3] - 2021-05-24
### Added
- Allow compilation on unsupported platforms.
  The code will panic on access just like in `std`.
  [@jcaesar](https://github.com/jcaesar)

## [0.2.2] - 2021-04-03
### Added
- `MmapOptions::populate`. [@adamreichold](https://github.com/adamreichold)

### Fixed
- Fix alignment computation for `flush_async` to match `flush`.
  [@adamreichold](https://github.com/adamreichold)

## [0.2.1] - 2021-02-08
### Added
- `MmapOptions::map_raw` and `MmapRaw`. [@diwic](https://github.com/diwic)

## [0.2.0] - 2020-12-19
### Changed
- MSRV is 1.31 now (edition 2018).
- Make anonymous memory maps private by default on unix. [@CensoredUsername](https://github.com/CensoredUsername)
- Add `map_copy_read_only`. [@zserik](https://github.com/zserik)

## 0.1.0 - 2020-01-18
### Added
- Fork [memmap-rs](https://github.com/danburkert/memmap-rs).

### Changed
- Use `LICENSE-APACHE` instead of `README.md` for some tests since it's immutable.

### Removed
- `winapi` dependency. [memmap-rs/pull/89](https://github.com/danburkert/memmap-rs/pull/89)

[Unreleased]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.3.1...HEAD
[0.3.1]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.2.3...v0.3.0
[0.2.3]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/RazrFalcon/memmap2-rs/compare/v0.1.0...v0.2.0
