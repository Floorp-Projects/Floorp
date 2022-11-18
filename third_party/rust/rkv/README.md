# rkv

[![CI Build Status](https://github.com/mozilla/rkv/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mozilla/rkv/actions/workflows/ci.yml)
[![Documentation](https://docs.rs/rkv/badge.svg)](https://docs.rs/rkv/)
[![Crate](https://img.shields.io/crates/v/rkv.svg)](https://crates.io/crates/rkv)

The [rkv Rust crate](https://crates.io/crates/rkv) is a simple, humane, typed key-value storage solution. It supports multiple backend engines with varying guarantees, such as [LMDB](http://www.lmdb.tech/doc/) for performance, or "SafeMode" for reliability.

## ⚠️ Warning ⚠️

To use rkv in production/release environments at Mozilla, you may do so with the "SafeMode" backend, for example:

```rust
use rkv::{Manager, Rkv};
use rkv::backend::{SafeMode, SafeModeEnvironment};

let mut manager = Manager::<SafeModeEnvironment>::singleton().write().unwrap();
let shared_rkv = manager.get_or_create(path, Rkv::new::<SafeMode>).unwrap();

...
```

The "SafeMode" backend performs well, with two caveats: the entire database is stored in memory, and write transactions are synchronously written to disk (only on commit).

In the future, it will be advisable to switch to a different backend with better performance guarantees. We're working on either fixing some LMDB crashes, or offering more choices of backend engines (e.g. SQLite).

## Use

Comprehensive information about using rkv is available in its [online documentation](https://docs.rs/rkv/), which can also be generated for local consumption:

```sh
cargo doc --open
```

## Build

Build this project as you would build other Rust crates:

```sh
cargo build
```

### Features

There are several features that you can opt-in and out of when using rkv:

By default, `db-dup-sort` and `db-int-key` features offer high level database APIs which allow multiple values per key, and optimizations around integer-based keys respectively. Opt out of these default features when specifying the rkv dependency in your Cargo.toml file to disable them; doing so avoids a certain amount of overhead required to support them.

To aid fuzzing efforts, `with-asan`, `with-fuzzer`, and `with-fuzzer-no-link` configure the build scripts responsible with compiling the underlying backing engines (e.g. LMDB) to build with these LLMV features enabled. Please refer to the official LLVM/Clang documentation on them for more informatiuon. These features are also disabled by default.

## Test

Test this project as you would test other Rust crates:

```sh
cargo test
```

The project includes unit and doc tests embedded in the `src/` files, integration tests in the `tests/` subdirectory, and usage examples in the `examples/` subdirectory. To ensure your changes don't break examples, also run them via the run-all-examples.sh shell script:

```sh
./run-all-examples.sh
```

Note: the test fixtures in the `tests/envs/` subdirectory aren't included in the package published to crates.io, so you must clone this repository in order to run the tests that depend on those fixtures or use the `rand` and `dump` executables to recreate them.

## Contribute

Of the various open source archetypes described in [A Framework for Purposeful Open Source](https://medium.com/mozilla-open-innovation/whats-your-open-source-strategy-here-are-10-answers-383221b3f9d3), the rkv project most closely resembles the Specialty Library, and we welcome contributions. Please report problems or ask questions using this repo's GitHub [issue tracker](https://github.com/mozilla/rkv/issues) and submit [pull requests](https://github.com/mozilla/rkv/pulls) for code and documentation changes.

rkv relies on the latest [rustfmt](https://github.com/rust-lang-nursery/rustfmt) for code formatting, so please make sure your pull request passes the rustfmt before submitting it for review. See rustfmt's [quick start](https://github.com/rust-lang-nursery/rustfmt#quick-start) for installation details.

We follow Mozilla's [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/) while contributing to this project.

## License

The rkv source code is licensed under the Apache License, Version 2.0, as described in the [LICENSE](https://github.com/mozilla/rkv/blob/master/LICENSE) file.
