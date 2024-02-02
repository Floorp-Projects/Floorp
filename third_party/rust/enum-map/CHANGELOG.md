<!--
SPDX-FileCopyrightText: 2018 - 2023 Kamila Borowska <kamila@borowska.pw>
SPDX-FileCopyrightText: 2021 Alex Sayers <alex@asayers.com>

SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Version 2.7.3

## Other changes

- Fixed [a regression introduced in 2.7.2 that caused `#[derive(Enum)]` to
  generate incorrect code when dealing with enums containing
  fields](https://codeberg.org/xfix/enum-map/issues/112).

# Version 2.7.2

## Other changes

- Reduced RAM usage and improved compilation times when using `derive(Enum)`
  for large enums with `overflow-checks` enabled.

# Version 2.7.1

## Other changes

- Updated author name.

# Version 2.7.0

## New features

- Implemented `EnumMap::from_fn`.

# Version 2.6.3

## Other changes

- Updated the repository URL as the project was migrated from GitHub
  to Codeberg.

- This project is now compliant with the REUSE Specification.

# Version 2.6.2

## Other changes

- Hide `out_of_bounds` reexport from documentation.

# Version 2.6.1

## Other changes

- Provide better panic message when providing out-of-bounds index
  to `Enum::from_usize``.

# Version 2.6.0

## New features

- `EnumMap::as_array` is now usable in const contexts.

## Other changes

- This crate now follows "N minus two" MSRV policy. This means that
  it supports the current Rust release, as well as the two before
  that.

- Upgraded syn to 2.0.0.

# Version 2.5.0

## New features

- Implemented `EnumMap::as_array` and `EnumMap::as_mut_array`
  (implemented by [@Fuuzetsu](https://github.com/Fuuzetsu)).

- Implemented `PartialOrd` and `Ord` for `EnumMap` (implemented by
  [@nicarran](https://github.com/nicarran)).

# Version 2.4.2

## Other changes

- Added license files to crate tarball.
- Added changelog to crate tarball.

# Version 2.4.1

## Other changes

- Improved performance of code generated for `from_usize` when
  deriving `Enum`.

# Version 2.4.0

## New features

- Implemented `Enum` for `()` (unit type) and `core::cmp::Ordering`
  (implemented by [@phimuemue](https://github.com/phimuemue)).

- Implemented `EnumMap::into_array`.

# Version 2.3.0

## New features

- `EnumMap::len` is now usable in const contexts.

## Other changes

- `Enum` derive now can deal with re-definitions of `usize` and
  `unimplemented`.

# Version 2.2.0

## New features

- `EnumMap::from_array` is now usable in const contexts.

# Version 2.1.0

## New features

- Implemented `DoubleEndedIterator` for `IntoIter`.

- Implemented `EnumMap::into_values`.

## Other changes

- Changed behavior of `IntoIter` so that it drops rest of the elements
  when one destructor panics.

# Version 2.0.3

## Other changes

- Optimized performance of `enum_map!` macro.

# Version 2.0.2

## Other changes

- Fixed safety problem when using `enum_map!` macro with enums that
  incorrectly implemented `Enum` trait.

# Version 2.0.1

## Other changes

- Adjusted crate metadata to avoid lib.rs warnings.

# Version 2.0.0

## New features

- Implemented `FromIterator` for `EnumMap` (implemented by @bit_network
  on GitLab).

- Implemented `EnumMap::map`.

- Derives support product types in addition to sum types (implemented
  by @bzim on GitLab).

- It's now possible to access enum length by accessing `LENGTH` in
  `Enum` trait.

## Breaking changes

- `Enum` trait was split into two traits, `Enum` and `EnumArray`.

# Version 1.1.1

## Other changes

- Worked around a bug in Clippy that caused false positives when using
  `use_self` lint for code that derived `Enum` trait.

# Version 1.1.0

## New features

- Implemented `Arbitrary` for maps where the value type also implements
  `Arbitrary`.  (You have to enable the "arbitrary" feature.)

# Version 1.0.0

## New features

- It's now possible to use `return` and `?` within `macro_rules!` macro.

- `Enum` trait is much simpler having two methods only.

## Other changes

- Removed previously deprecated features.

- Renamed `to_usize` to `into_usize` matching the naming convention
  used in Rust programming language.

# Version 0.6.5

## Other changes

- Deprecated `EnumMap::is_empty` and `EnumMap::new`. `EnumMap::new` usages
  can be replaced with `EnumMap::default`.

# Version 0.6.4

## Other changes

- Deprecated `EnumMap::as_ptr` and `EnumMap::as_mut_ptr`.

# Version 0.6.3

## New features

- `Iter` and `Values` now implements `Clone` (added by @amanieu).

# Version 0.6.2.

## New features

- Added `EnumMap#clear` method (added by @Riey, thanks :)).

# Version 0.6.0

## Incompatible changes

- Now requires Rust 1.36.

# Version 0.5.0

- Fixed the issue where an aliasing `From` trait implementation caused
  compilation errors with `enum_map!` macro.

## Incompatible changes

- Now requires Rust 1.31.

# Version 0.4.1

## New features

- Default `serde` features are disabled. This allows enabling serde feature when
  compiling without `std`.

# Version 0.4.0

Change of `#[derive(EnumMap)]` to `#[derive(Enum)]` was supposed to appear in 0.3.0,
but it was forgotten about. This release fixes just that.

## Incompatible changes

- Changed `#[derive(EnumMap)]` to `#[derive(Enum)]` to match trait name.

# Version 0.3.1

- Updated README use `#[derive(EnumMap)]` instead of `#[derive(Enum)]`.

# Version 0.3.0

## New features

- Implemented compact serde serialization for binary formats like bincode.

- Iterator traits with exception now implement `FusedIterator`.

## Incompatible changes

- Increased required Rust version to 1.26.0.

- Renamed `Internal` trait to `Enum`.

- Added new associated constant `POSSIBLE_VALUES` to `Enum` trait,
  representing the number of possible values the type can have. Manual
  implementations are required to provide it.

- Removed `Enum` implementation for `Option<T>`.

- Implemented compact serialization, for formats like `bincode`. This
  makes it impossible to deserialize non-compact representation used by
  enum-map 0.2.0.

- `values` method returns `Values<V>` as opposed to `slice::Iter<V>`.
