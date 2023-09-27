# [Unreleased]

# [0.3.3] - 2023-08-28

- Disable "libc/std" in no-std configurations.
  [#77](https://github.com/lambda-fairy/rust-errno/pull/77)

- Bump errno-dragonfly to 0.1.2
  [#75](https://github.com/lambda-fairy/rust-errno/pull/75)

- Support for the ESP-IDF framework
  [#74](https://github.com/lambda-fairy/rust-errno/pull/74)

# [0.3.2] - 2023-07-30

- Fix build on Hermit
  [#73](https://github.com/lambda-fairy/rust-errno/pull/73)

- Add support for QNX Neutrino
  [#72](https://github.com/lambda-fairy/rust-errno/pull/72)

# [0.3.1] - 2023-04-08

- Correct link name on redox
  [#69](https://github.com/lambda-fairy/rust-errno/pull/69)

- Update windows-sys requirement from 0.45 to 0.48
  [#70](https://github.com/lambda-fairy/rust-errno/pull/70)

# [0.3.0] - 2023-02-12

- Add haiku support
  [#42](https://github.com/lambda-fairy/rust-errno/pull/42)

- Add AIX support
  [#54](https://github.com/lambda-fairy/rust-errno/pull/54)

- Add formatting with `#![no_std]`
  [#44](https://github.com/lambda-fairy/rust-errno/pull/44)

- Switch from `winapi` to `windows-sys` [#55](https://github.com/lambda-fairy/rust-errno/pull/55)

- Update minimum Rust version to 1.48
  [#48](https://github.com/lambda-fairy/rust-errno/pull/48) [#55](https://github.com/lambda-fairy/rust-errno/pull/55)

- Upgrade to Rust 2018 edition [#59](https://github.com/lambda-fairy/rust-errno/pull/59)

- wasm32-wasi: Use `__errno_location` instead of `feature(thread_local)`. [#66](https://github.com/lambda-fairy/rust-errno/pull/66)

# [0.2.8] - 2021-10-27

- Optionally support no_std
  [#31](https://github.com/lambda-fairy/rust-errno/pull/31)

[Unreleased]: https://github.com/lambda-fairy/rust-errno/compare/v0.3.3...HEAD
[0.3.3]: https://github.com/lambda-fairy/rust-errno/compare/v0.3.1...v0.3.2
[0.3.2]: https://github.com/lambda-fairy/rust-errno/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/lambda-fairy/rust-errno/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/lambda-fairy/rust-errno/compare/v0.2.8...v0.3.0
[0.2.8]: https://github.com/lambda-fairy/rust-errno/compare/v0.2.7...v0.2.8
