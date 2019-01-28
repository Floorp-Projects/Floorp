# Change Log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## 0.13.0 - 2018-10-19
- Updated to `syn` v0.15
- Extended Display-like derives to support custom formats

## 0.12.0 - 2018-09-19

### Changed
- Updated to `syn` v0.14, `quote` v0.6 and `proc-macro2` v0.4

## 0.11.0 - 2018-05-12

### Changed
- Updated to latest version of `syn` and `quote`

### Fixed
- Changed some URLs in the docs so they were correct on crates.io and docs.rs
- The `Result` type is now referenced in the derives using its absolute path
  (`::std::result::Result`) to make sure that the derives don't accidentally use
  another `Result` type that is in scope.

## 0.10.0 - 2018-03-29

### Added
- Allow deriving of `TryInto`
- Allow deriving of `Deref`
- Allow deriving of `DerefMut`

## 0.9.0 - 2018-03-18

### Added
- Allow deriving of `Display`, `Binary`, `Octal`, `LowerHex`, `UpperHex`, `LowerExp`, `UpperExp`, `Pointer`
- Allow deriving of `Index`
- Allow deriving of `IndexMut`

### Fixed
- Allow cross crate inlining of derived methods

### Internal changes
- Fix most `clippy` warnings

## 0.8.0 - 2018-03-10

### Added
- Allow deriving of `FromStr`

### Changed
- Updated to latest version of `syn` and `quote`

## 0.7.1 - 2018-01-25

### Fixed
- Add `#[allow(missing_docs)]` to the Constructor definition

### Internal changes
- Run `rustfmt` on the code


## 0.7.0 - 2017-07-25

### Changed
- Changed code to work with newer version of the `syn` library.

## 0.6.2 - 2017-04-23

### Changed
- Deriving `From`, `Into` and `Constructor` now works for empty structs.


## 0.6.1 - 2017-03-08

### Changed
- The `new()` method that is created when deriving `Constructor` is now public.
  This makes it a lot more useful.


## 0.6.0 - 2017-02-20

### Added

- Derives for `Into`, `Constructor` and `MulAssign`-like

### Changed

- `From` is now derived for enum variants with multiple fields.

### Fixed

- Derivations now support generics.


## 0.5.0 - 2017-02-02

### Added

- Lots of docs.
- Derives for `Neg`-like and `AddAssign`-like.

### Changed
- `From` can now be derived for structs with multiple fields.
