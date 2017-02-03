# pkg-config-rs

[![Build Status](https://travis-ci.org/alexcrichton/pkg-config-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/pkg-config-rs)

[Documentation](https://docs.rs/pkg-config)

A simple library meant to be used as a build dependency with Cargo packages in
order to use the system `pkg-config` tool (if available) to determine where a
library is located.

You can use this crate directly to probe for specific libraries, or use
[metadeps](https://github.com/joshtriplett/metadeps) to declare all your
`pkg-config` dependencies in `Cargo.toml`.

# Example

Find the system library named `foo`, with minimum version 1.2.3:

```rust
extern crate pkg_config;

fn main() {
    pkg_config::Config::new().atleast_version("1.2.3").probe("foo").unwrap();
}
```

Find the system library named `foo`, with no version requirement (not
recommended):

```rust
extern crate pkg_config;

fn main() {
    pkg_config::probe_library("foo").unwrap();
}
```

# License

`pkg-config-rs` is primarily distributed under the terms of both the MIT
license and the Apache License (Version 2.0), with portions covered by various
BSD-like licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.
