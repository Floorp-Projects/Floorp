# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## 1.5.0 - 2017-02-05
### Change

* Reverse the order of record values in full mode to match slog 1.5
  definition

## 1.4.0 - 2017-01-29
### Changed

* Fix a bug in `new_plain` that would make it still use colors.
* No comma will be printed after an empty "msg" field
* Changed order of full format values

## 1.3.5 - 2016-01-13
### Fixed

* [1.3.4 with `?` operator breaks semver](https://github.com/slog-rs/term/issues/6) - fix allows builds on stable Rust back to 1.11

## 1.3.4 - 2016-12-27
### Fixed

* [Fix compact formatting grouping messages incorrectly](https://github.com/slog-rs/slog/issues/90)

## 1.3.3 - 2016-10-31
### Changed

* Added `Send+Sync` to `Drain` returned on `build`

## 1.3.2 - 2016-10-22
### Changed

* Fix compact format, repeating some values unnecessarily.

## 1.3.1 - 2016-10-22
### Changed

* Make `Format` public so it can be reused

## 1.3.0 - 2016-10-21
### Changed

* **BREAKING**: Switched `AsyncStramer` to `slog_extra::Async`

## 1.2.0 - 2016-10-17
### Changed

* **BREAKING**: Rewrite handling of owned values.

## 1.1.0 - 2016-09-28
### Added

* Custom timestamp function support

### Changed

* Logging level color uses only first 8 ANSI terminal colors for better compatibility

## 1.0.0 - 2016-09-21

First stable release.
