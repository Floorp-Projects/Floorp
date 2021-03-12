# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org).

<!--
Note: In this file, do not use the hard wrap in the middle of a sentence for compatibility with GitHub comment style markdown rendering.
-->

## [Unreleased]

## [0.4.5] - 2021-01-05

- Exclude unneeded files from crates.io.

## [0.4.4] - 2020-11-02

- [Accept `const_fn` attribute with no arguments and functions without `const` keyword.](https://github.com/taiki-e/const_fn/pull/34)
  This allows `const_fn` to be used as an optional dependency.

  ```rust
  #[cfg_attr(feature = "...", const_fn::const_fn)]
  pub fn func() {
      /* ... */
  }
  ```

## [0.4.3] - 2020-11-02

- [`const_fn` no longer fails to compile if unable to determine rustc version. Instead, it now displays a warning.](https://github.com/taiki-e/const_fn/pull/31)

- [`const_fn` no longer relies on debug print format.](https://github.com/taiki-e/const_fn/pull/30)

## [0.4.2] - 2020-08-31

- [Improve error messages when failed to parse version information.](https://github.com/taiki-e/const_fn/pull/26)

- [Fix compile failure with cargo installed by yum.](https://github.com/taiki-e/const_fn/pull/26)

## [0.4.1] - 2020-08-25

- [Fix compile failure with non-cargo build systems.](https://github.com/taiki-e/const_fn/pull/23)

## [0.4.0] - 2020-08-25

- [Add support for version-based code generation.](https://github.com/taiki-e/const_fn/pull/17) The following conditions are available:

  ```rust
  use const_fn::const_fn;

  // function is `const` on specified version and later compiler (including beta and nightly)
  #[const_fn("1.36")]
  pub const fn version() {
      /* ... */
  }

  // function is `const` on nightly compiler (including dev build)
  #[const_fn(nightly)]
  pub const fn nightly() {
      /* ... */
  }

  // function is `const` if `cfg(...)` is true
  #[const_fn(cfg(...))]
  pub const fn cfg() {
      /* ... */
  }

  // function is `const` if `cfg(feature = "...")` is true
  #[const_fn(feature = "...")]
  pub const fn feature() {
      /* ... */
  }
  ```

- Improve compile time by removing proc-macro related dependencies ([#18](https://github.com/taiki-e/const_fn/pull/18), [#20](https://github.com/taiki-e/const_fn/pull/20)).

## [0.3.1] - 2019-12-09

- Updated `syn-mid` to 0.5.

## [0.3.0] - 2019-10-20

- `#[const_fn]` attribute may only be used on const functions.

## [0.2.1] - 2019-08-15

- Updated `proc-macro2`, `syn`, and `quote` to 1.0.

- Updated `syn-mid` to 0.4.

## [0.2.0] - 2019-06-16

- Transition to Rust 2018. With this change, the minimum required version will go up to Rust 1.31.

## [0.1.7] - 2019-02-18

- Update syn-mid version to 0.3

## [0.1.6] - 2019-02-15

- Reduce compilation time

## [0.1.5] - 2019-02-15

- Revert 0.1.4

## [0.1.4] - 2019-02-15

**Note: This release has been yanked.**

- Reduce compilation time

## [0.1.3] - 2019-01-06

- Fix dependencies

## [0.1.2] - 2018-12-27

- Improve error messages

## [0.1.1] - 2018-12-27

- Improve an error message

## [0.1.0] - 2018-12-25

Initial release

[Unreleased]: https://github.com/taiki-e/const_fn/compare/v0.4.5...HEAD
[0.4.5]: https://github.com/taiki-e/const_fn/compare/v0.4.4...v0.4.5
[0.4.4]: https://github.com/taiki-e/const_fn/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/taiki-e/const_fn/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/taiki-e/const_fn/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/taiki-e/const_fn/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/taiki-e/const_fn/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/taiki-e/const_fn/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/taiki-e/const_fn/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/taiki-e/const_fn/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/taiki-e/const_fn/compare/v0.1.7...v0.2.0
[0.1.7]: https://github.com/taiki-e/const_fn/compare/v0.1.6...v0.1.7
[0.1.6]: https://github.com/taiki-e/const_fn/compare/v0.1.5...v0.1.6
[0.1.5]: https://github.com/taiki-e/const_fn/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/taiki-e/const_fn/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/taiki-e/const_fn/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/taiki-e/const_fn/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/taiki-e/const_fn/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/taiki-e/const_fn/releases/tag/v0.1.0
