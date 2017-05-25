# backtrace-rs

[![Build Status](https://travis-ci.org/alexcrichton/backtrace-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/backtrace-rs)
[![Build status](https://ci.appveyor.com/api/projects/status/v4l9oj4aqbbgyx44?svg=true)](https://ci.appveyor.com/project/alexcrichton/backtrace-rs)

[Documentation](http://alexcrichton.com/backtrace-rs)

A library for acquiring backtraces at runtime for Rust. This library aims to
enhance the support given by the standard library at `std::rt` by providing a
more stable and programmatic interface.

## Install

```toml
[dependencies]
backtrace = "0.3"
```

```rust
extern crate backtrace;
```

Note that this crate requires `make`, `objcopy`, and `ar` to be present on Linux
systems.

## Usage

To simply capture a backtrace and defer dealing with it until a later time,
you can use the top-level `Backtrace` type.

```rust
extern crate backtrace;

use backtrace::Backtrace;

fn main() {
    let bt = Backtrace::new();

    // do_some_work();

    println!("{:?}", bt);
}
```

If, however, you'd like more raw access to the actual tracing functionality, you
can use the `trace` and `resolve` functions directly.

```rust
extern crate backtrace;

fn main() {
    backtrace::trace(|frame| {
        let ip = frame.ip();
        let symbol_address = frame.symbol_address();

        // Resolve this instruction pointer to a symbol name
        backtrace::resolve(ip, |symbol| {
            if let Some(name) = symbol.name() {
                // ...
            }
            if let Some(filename) = symbol.filename() {
                // ...
            }
        });

        true // keep going to the next frame
    });
}
```

## Platform Support

This library currently supports OSX, Linux, and Windows. Support for other
platforms is always welcome!

## License

`backtrace-rs` is primarily distributed under the terms of both the MIT license
and the Apache License (Version 2.0), with portions covered by various BSD-like
licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.
