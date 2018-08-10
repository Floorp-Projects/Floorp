[![Travis CI Build Status](https://travis-ci.org/mozilla/rkv.svg?branch=master)](https://travis-ci.org/mozilla/rkv)
[![Appveyor Build Status](https://ci.appveyor.com/api/projects/status/lk936u5y5bi6qafb/branch/master?svg=true)](https://ci.appveyor.com/project/mykmelez/rkv/branch/master)
[![Documentation](https://docs.rs/rkv/badge.svg)](https://docs.rs/rkv/)
[![Crate](https://img.shields.io/crates/v/rkv.svg)](https://crates.io/crates/rkv)

# rkv

The [rkv Rust crate](https://crates.io/crates/rkv) is a simple, humane, typed Rust interface to [LMDB](http://www.lmdb.tech/doc/).

## Use

Comprehensive information about using rkv is available in its [online documentation](https://docs.rs/rkv/), which you can also generate for local consumption:

```sh
cargo doc --open
```

## Build

Build this project as you would build other Rust crates:

```sh
cargo build
```

If you specify the `backtrace` feature, backtraces will be enabled in `failure`
errors. This feature is disabled by default.

## Test

Test this project as you would test other Rust crates:

```sh
cargo test
```

The project includes unit and doc tests embedded in the `src/` files, integration tests in the `tests/` subdirectory, and usage examples in the `examples/` subdirectory. To ensure your changes don't break examples, also run them via the run-all-examples.sh shell script:

```sh
./run-all-examples.sh
```

## Contribute

Of the various open source archetypes described in [A Framework for Purposeful Open Source](https://medium.com/mozilla-open-innovation/whats-your-open-source-strategy-here-are-10-answers-383221b3f9d3), the rkv project most closely resembles the Specialty Library, and we welcome contributions. Please report problems or ask questions using this repo's GitHub [issue tracker](https://github.com/mozilla/rkv/issues) and submit [pull requests](https://github.com/mozilla/rkv/pulls) for code and documentation changes.

rkv relies on the latest [rustfmt](https://github.com/rust-lang-nursery/rustfmt) for code formatting, so please make sure your pull request passes the rustfmt before submitting it for review. See rustfmt's [quick start](https://github.com/rust-lang-nursery/rustfmt#quick-start) for installation details.

We follow Mozilla's [Community Participation Guidelines](https://www.mozilla.org/en-US/about/governance/policies/participation/) while contributing to this project.

## License

The rkv source code is licensed under the Apache License, Version 2.0, as described in the [LICENSE](https://github.com/mozilla/rkv/blob/master/LICENSE) file.
