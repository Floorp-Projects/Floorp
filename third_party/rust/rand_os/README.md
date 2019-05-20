# rand_os

[![Build Status](https://travis-ci.org/rust-random/rand.svg?branch=master)](https://travis-ci.org/rust-random/rand)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/rust-random/rand?svg=true)](https://ci.appveyor.com/project/rust-random/rand)
[![Latest version](https://img.shields.io/crates/v/rand_os.svg)](https://crates.io/crates/rand_os)
[![Book](https://img.shields.io/badge/book-master-yellow.svg)](https://rust-random.github.io/book/)
[![API](https://img.shields.io/badge/api-master-yellow.svg)](https://rust-random.github.io/rand/rand_os)
[![API](https://docs.rs/rand_os/badge.svg)](https://docs.rs/rand_os)
[![Minimum rustc version](https://img.shields.io/badge/rustc-1.22+-lightgray.svg)](https://github.com/rust-random/rand#rust-version-requirements)

A random number generator that retrieves randomness straight from the
operating system.

This crate depends on [rand_core](https://crates.io/crates/rand_core) and is
part of the [Rand project](https://github.com/rust-random/rand).

This crate aims to support all of Rust's `std` platforms with a system-provided
entropy source. Unlike other Rand crates, this crate does not support `no_std`
(handling this gracefully is a current discussion topic).

Links:

-   [API documentation (master)](https://rust-random.github.io/rand/rand_os)
-   [API documentation (docs.rs)](https://docs.rs/rand_os)
-   [Changelog](CHANGELOG.md)

## License

`rand_os` is distributed under the terms of both the MIT license and the
Apache License (Version 2.0).

See [LICENSE-APACHE](LICENSE-APACHE) and [LICENSE-MIT](LICENSE-MIT), and
[COPYRIGHT](COPYRIGHT) for details.
