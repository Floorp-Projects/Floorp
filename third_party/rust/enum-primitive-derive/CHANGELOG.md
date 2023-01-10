# ChangeLog

## 0.2.2
### Changed
- Switch to using core instead of std for no-std support

## 0.2.1
### Added
- support TryFrom

## 0.2.0 (yanked)
### Changed
- Upgrade [syn](https://crates.io/crates/syn) and [quote](https://crates.io/crates/quote) to 1.0
- add a better diagnostic for the case where a discriminant isn't specified for
	an enum
- Move unnecessary [`num-traits`](https://crates.io/crates/num-traits) dependency to `dev-dependencies`
- Migrate to Rust 2018 edition

## 0.1.2

### Changed

- drop `extern crate core;` as core is unused

## 0.1.1

### Added

- Support for more casts on discriminants

## 0.1.0

Initial version
