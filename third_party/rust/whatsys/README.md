# `whatsys` -- What's this?

[![Crates.io version](https://img.shields.io/crates/v/whatsys.svg?style=flat-square)](https://crates.io/crates/whatsys)
[![docs.rs docs](https://img.shields.io/badge/docs-latest-blue.svg?style=flat-square)](https://docs.rs/whatsys)
[![License: MIT](https://img.shields.io/github/license/badboy/whatsys?style=flat-square)](LICENSE)
[![Build Status](https://img.shields.io/github/actions/workflow/status/badboy/whatsys/ci.yml?branch=main&style=flat-square)](https://github.com/badboy/whatsys/actions/workflows/ci.yml?query=workflow%3ACI)

What kernel version is running?

# Example

```rust
let kernel = whatsys::kernel_version(); // E.g. Some("20.3.0")
```

# Supported operating systems

We support the following operating systems:

* Windows
* macOS
* Linux
* Android

# License

MIT. See [LICENSE](LICENSE).

Based on:

* [sys-info](https://crates.io/crates/sys-info), [Repository](https://github.com/FillZpp/sys-info-rs), [MIT LICENSE][sys-info-mit]
* [sysinfo](https://crates.io/crates/sysinfo), [Repository](https://github.com/GuillaumeGomez/sysinfo), [MIT LICENSE][sysinfo-mit]

[sys-info-mit]: https://github.com/FillZpp/sys-info-rs/blob/master/LICENSE
[sysinfo-mit]: https://github.com/GuillaumeGomez/sysinfo/blob/master/LICENSE
