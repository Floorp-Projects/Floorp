# cmake

[![Build Status](https://travis-ci.org/alexcrichton/cmake-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/cmake-rs)

[Documentation](http://alexcrichton.com/cmake-rs)

A build dependency for running the `cmake` build tool to compile a native
library.

```toml
# Cargo.toml
[build-dependencies]
cmake = "0.2"
```

The CMake executable is assumed to be `cmake` unless the `CMAKE`
environmental variable is set.

# License

This project is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
