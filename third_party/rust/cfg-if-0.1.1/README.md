# cfg-if

[![Build Status](https://travis-ci.org/alexcrichton/cfg-if.svg?branch=master)](https://travis-ci.org/alexcrichton/cfg-if)

[Documentation](http://alexcrichton.com/cfg-if)

A macro to ergonomically define an item depending on a large number of #[cfg]
parameters. Structured like an if-else chain, the first matching branch is the
item that gets emitted.

```toml
[dependencies]
cfg-if = "0.1"
```

## Example

```rust
#[macro_use]
extern crate cfg_if;

cfg_if! {
    if #[cfg(unix)] {
        fn foo() { /* unix specific functionality */ }
    } else if #[cfg(target_pointer_width = "32")] {
        fn foo() { /* non-unix, 32-bit functionality */ }
    } else {
        fn foo() { /* fallback implementation */ }
    }
}

fn main() {
    foo();
}
```

# License

`cfg-if` is primarily distributed under the terms of both the MIT license and
the Apache License (Version 2.0), with portions covered by various BSD-like
licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.

