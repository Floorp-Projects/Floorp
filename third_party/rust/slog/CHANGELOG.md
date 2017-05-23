**Note:** `slog-*` sub-crates have their own ChangeLog files under
corresponding `crates/*/` directory.

# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## 1.5.0 - 2017-01-19
### Changed

* Order of key-value pairs is now strictly defined

## 1.4.1 - 2017-01-19
### Changed

* Fix an invalid syntax exposed by nightly rust change (Issue #103)

## 1.4.0 - 2016-12-27
### Changed

* Deprecate `OwnedKeyValueList::id`
* Updated documentation

## 1.3.2 - 2016-11-19
### Added

* `slog_o` as an alternative name for `o`

## 1.3.1 - 2016-11-19

Cargo publishing mistake.

## 1.3.0 - 2016-10-31
### Changed

* **BREAKING**: Removed `Send+Sync` from `Drain`

## 1.2.1 - 2016-10-27
### Added

* `OwnedKeyValueList::id` for owned key value unique identification

## 1.2.0 - 2016-10-21
### Changed

* **BREAKING**: `Serializer` takes `key : &'static str` now
* Fixed corner cases in `info!(...)` and other macros

## 1.1.0 - 2016-10-17
### Changed

* **BREAKING**: Rewrite handling of owned values.

## 1.0.1
### Changed

* Fix `use std` in `o!`
* Implement `fmt::Debug` for `Logger`

## 1.0.0 - 2016-09-21

First stable release.
