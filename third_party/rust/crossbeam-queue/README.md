# Crossbeam Queue

[![Build Status](https://travis-ci.org/crossbeam-rs/crossbeam.svg?branch=master)](
https://travis-ci.org/crossbeam-rs/crossbeam)
[![License](https://img.shields.io/badge/license-MIT%2FApache--2.0-blue.svg)](
https://github.com/crossbeam-rs/crossbeam-queue/tree/master/src)
[![Cargo](https://img.shields.io/crates/v/crossbeam-queue.svg)](
https://crates.io/crates/crossbeam-queue)
[![Documentation](https://docs.rs/crossbeam-queue/badge.svg)](
https://docs.rs/crossbeam-queue)
[![Rust 1.26+](https://img.shields.io/badge/rust-1.26+-lightgray.svg)](
https://www.rust-lang.org)

This crate provides concurrent queues that can be shared among threads:

* [`ArrayQueue`], a bounded MPMC queue that allocates a fixed-capacity buffer on construction.
* [`SegQueue`], an unbounded MPMC queue that allocates small buffers, segments, on demand.

[`ArrayQueue`]: https://docs.rs/crossbeam-queue/*/crossbeam_queue/struct.ArrayQueue.html
[`SegQueue`]: https://docs.rs/crossbeam-queue/*/crossbeam_queue/struct.SegQueue.html

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
crossbeam-queue = "0.1"
```

Next, add this to your crate:

```rust
extern crate crossbeam_queue;
```

## Compatibility

The minimum supported Rust version is 1.26.

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

#### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.

#### Third party software

This product includes copies and modifications of software developed by third parties:

* [src/array_queue.rs](src/array_queue.rs) is based on
  [Bounded MPMC queue](http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)
  by Dmitry Vyukov, licensed under the Simplified BSD License and the Apache License, Version 2.0.

See the source code files for more details.

Copies of third party licenses can be found in [LICENSE-THIRD-PARTY](LICENSE-THIRD-PARTY).
