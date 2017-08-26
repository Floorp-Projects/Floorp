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

`mio-uds` is primarily distributed under the terms of both the MIT license and
the Apache License (Version 2.0), with portions covered by various BSD-like
licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.

