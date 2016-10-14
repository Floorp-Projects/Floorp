# pkg-config-rs

[![Build Status](https://travis-ci.org/alexcrichton/pkg-config-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/pkg-config-rs)

[Documentation](http://alexcrichton.com/pkg-config-rs)

A simple library meant to be used as a build dependency with Cargo packages in
order to use the system `pkg-config` tool (if available) to determine where a
library is located.

```rust
extern crate pkg_config;

fn main() {
    pkg_config::find_library("foo").unwrap();
}
```

# License

`pkg-config-rs` is primarily distributed under the terms of both the MIT
license and the Apache License (Version 2.0), with portions covered by various
BSD-like licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.
