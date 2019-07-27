# mio-named-pipes

[![Build status](https://ci.appveyor.com/api/projects/status/y0ct01srewnhhesn?svg=true)](https://ci.appveyor.com/project/alexcrichton/mio-named-pipes)

[Documentation](https://docs.rs/mio-named-pipes/0.1/x86_64-pc-windows-msvc/mio_named_pipes/)

A library for integrating Windows [Named Pipes] with [mio].

[Named Pipes]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365590(v=vs.85).aspx
[mio]: https://github.com/carllerche/mio

```toml
# Cargo.toml
[dependencies]
mio-named-pipes = "0.6"
mio = "0.6"
```

## Usage

The primary type, `NamedPipe`, can be constructed with `NamedPipe::new` or
through the `IntoRawHandle` type. All operations on `NamedPipe` are nonblocking
and will return an I/O error if they'd block (with the error indicating so).

Typically you can use a `NamedPipe` in the same way you would a TCP socket on
Windows with mio.

> **Note**: Named pipes on Windows do not have a zero-cost abstraction when
> working with the mio interface (readiness, not completion). As a result, this
> library internally has some buffer management that hasn't been optimized yet.
> It's recommended you benchmark this library for your application, and feel
> free to contact me if anything looks awry.

# License

`mio-named-pipes` is primarily distributed under the terms of both the MIT
license and the Apache License (Version 2.0), with portions covered by various
BSD-like licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.


