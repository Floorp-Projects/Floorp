# rand_xorshift

[![Build Status](https://travis-ci.org/rust-random/rand.svg)](https://travis-ci.org/rust-random/rand)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/rust-random/rand?svg=true)](https://ci.appveyor.com/project/rust-random/rand)
[![Latest version](https://img.shields.io/crates/v/rand_xorshift.svg)](https://crates.io/crates/rand_xorshift)
[![Book](https://img.shields.io/badge/book-master-yellow.svg)](https://rust-random.github.io/book/)
[![API](https://img.shields.io/badge/api-master-yellow.svg)](https://rust-random.github.io/rand/rand_xorshift)
[![API](https://docs.rs/rand_xorshift/badge.svg)](https://docs.rs/rand_xorshift)
[![Minimum rustc version](https://img.shields.io/badge/rustc-1.22+-lightgray.svg)](https://github.com/rust-random/rand#rust-version-requirements)

Implements the Xorshift random number generator.

The Xorshift[^1] algorithm is not suitable for cryptographic purposes
but is very fast. If you do not know for sure that it fits your
requirements, use a more secure one such as `StdRng` or `OsRng`.

[^1]: Marsaglia, George (July 2003).
      ["Xorshift RNGs"](https://www.jstatsoft.org/v08/i14/paper).
      *Journal of Statistical Software*. Vol. 8 (Issue 14).

Links:

-   [API documentation (master)](https://rust-random.github.io/rand/rand_xorshift)
-   [API documentation (docs.rs)](https://docs.rs/rand_xorshift)
-   [Changelog](CHANGELOG.md)

[rand]: https://crates.io/crates/rand


## Crate Features

`rand_xorshift` is `no_std` compatible. It does not require any functionality
outside of the `core` lib, thus there are no features to configure.

The `serde1` feature includes implementations of `Serialize` and `Deserialize`
for the included RNGs.


## License

`rand_xorshift` is distributed under the terms of both the MIT license and the
Apache License (Version 2.0).

See [LICENSE-APACHE](LICENSE-APACHE) and [LICENSE-MIT](LICENSE-MIT), and
[COPYRIGHT](COPYRIGHT) for details.
