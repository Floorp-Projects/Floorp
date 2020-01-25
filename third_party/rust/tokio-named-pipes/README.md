# tokio-named-pipes
[![Build status](https://ci.appveyor.com/api/projects/status/motwon3ro35xwb2x?svg=true)](https://ci.appveyor.com/project/NikolayVolf/tokio-named-pipes)

[Documentation](http://alexcrichton.com/tokio-named-pipes)

A library for integrating Windows [Named Pipes] with [tokio].

[Named Pipes]: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365590(v=vs.85).aspx
[tokio]: https://github.com/tokio-rs/tokio

```toml
# Cargo.toml
[dependencies]
tokio-named-pipes = { git = "https://github.com/NikVolf/tokio-named-pipes", branch = "stable" }
```

Next, add this to your crate:

```rust
extern crate tokio_named_pipes;
```

# License

`tokio-named-pipes` is primarily distributed under the terms of both the MIT
license and the Apache License (Version 2.0), with portions covered by various
BSD-like licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.


