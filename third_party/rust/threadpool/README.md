# threadpool

A thread pool for running a number of jobs on a fixed set of worker threads.

[![Build Status](https://travis-ci.org/frewsxcv/rust-threadpool.svg?branch=master)](https://travis-ci.org/frewsxcv/rust-threadpool)

[Documentation](https://frewsxcv.github.io/rust-threadpool/threadpool/index.html)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
threadpool = "1.0"
```

and this to your crate root:

```rust
extern crate threadpool;
```

## Minimal requirements

This crate requires Rust >= 1.9.0

## Similar libraries

* [rust-scoped-pool](http://github.com/reem/rust-scoped-pool)
* [scoped-threadpool-rs](https://github.com/Kimundi/scoped-threadpool-rs)
* [crossbeam](https://github.com/aturon/crossbeam)

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
