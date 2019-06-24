# mio-uds

[![Build Status](https://travis-ci.org/alexcrichton/mio-uds.svg?branch=master)](https://travis-ci.org/alexcrichton/mio-uds)

[Documentation](https://docs.rs/mio-uds)

A library for integrating Unix Domain Sockets with [mio]. Based on the standard
library's [support for Unix sockets][std], except all of the abstractions and
types are nonblocking to conform with the expectations of mio.

[mio]: https://github.com/carllerche/mio
[std]: https://doc.rust-lang.org/std/os/unix/net/

```toml
# Cargo.toml
[dependencies]
mio-uds = "0.6"
mio = "0.6"
```

## Usage

The three exported types at the top level, `UnixStream`, `UnixListener`, and
`UnixDatagram`, are thin wrappers around the libstd counterparts. They can be
used in similar fashions to mio's TCP and UDP types in terms of registration and
API.

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
