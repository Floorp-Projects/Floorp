# Ryū

[![Build Status](https://api.travis-ci.org/dtolnay/ryu.svg?branch=master)](https://travis-ci.org/dtolnay/ryu)
[![Latest Version](https://img.shields.io/crates/v/ryu.svg)](https://crates.io/crates/ryu)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://docs.rs/ryu)
[![Rustc Version 1.15+](https://img.shields.io/badge/rustc-1.15+-lightgray.svg)](https://blog.rust-lang.org/2017/02/02/Rust-1.15.html)

Pure Rust implementation of Ryū, an algorithm to quickly convert floating point
numbers to decimal strings.

The PLDI'18 paper [*Ryū: fast float-to-string conversion*][paper] by Ulf Adams
includes a complete correctness proof of the algorithm. The paper is available
under the creative commons CC-BY-SA license.

This Rust implementation is a line-by-line port of Ulf Adams' implementation in
C, [https://github.com/ulfjack/ryu][upstream].

*Requirements: this crate supports any compiler version back to rustc 1.15; it
uses nothing from the Rust standard library so is usable from no_std crates.*

[paper]: https://dl.acm.org/citation.cfm?id=3192369
[upstream]: https://github.com/ulfjack/ryu/tree/688f43b62276b400728baad54afc32c3ab9c1a95

```toml
[dependencies]
ryu = "1.0"
```

## Example

```rust
fn main() {
    let mut buffer = ryu::Buffer::new();
    let printed = buffer.format(1.234);
    assert_eq!(printed, "1.234");
}
```

## Performance

You can run upstream's benchmarks with:

```console
$ git clone https://github.com/ulfjack/ryu c-ryu
$ cd c-ryu
$ bazel run -c opt //ryu/benchmark
```

And the same benchmark against our implementation with:

```console
$ git clone https://github.com/dtolnay/ryu rust-ryu
$ cd rust-ryu
$ cargo run --example upstream_benchmark --release
```

These benchmarks measure the average time to print a 32-bit float and average
time to print a 64-bit float, where the inputs are distributed as uniform random
bit patterns 32 and 64 bits wide.

The upstream C code, the unsafe direct Rust port, and the safe pretty Rust API
all perform the same, taking around 21 nanoseconds to format a 32-bit float and
31 nanoseconds to format a 64-bit float.

There is also a Rust-specific benchmark comparing this implementation to the
standard library which you can run with:

```console
$ cargo bench
```

The benchmark shows Ryu approximately 4-10x faster than the standard library
across a range of f32 and f64 inputs. Measurements are in nanoseconds per
iteration; smaller is better.

| type=f32 | 0.0  | 0.1234 | 2.718281828459045 | f32::MAX |
|:--------:|:----:|:------:|:-----------------:|:--------:|
| RYU      | 3ns  | 28ns   | 23ns              | 22ns     |
| STD      | 40ns | 106ns  | 128ns             | 110ns    |

| type=f64 | 0.0  | 0.1234 | 2.718281828459045 | f64::MAX |
|:--------:|:----:|:------:|:-----------------:|:--------:|
| RYU      | 3ns  | 50ns   | 35ns              | 32ns     |
| STD      | 39ns | 105ns  | 128ns             | 202ns    |

## Formatting

This library tends to produce more human-readable output than the standard
library's to\_string, which never uses scientific notation. Here are two
examples:

- *ryu:* 1.23e40, *std:* 12300000000000000000000000000000000000000
- *ryu:* 1.23e-40, *std:* 0.000000000000000000000000000000000000000123

Both libraries print short decimals such as 0.0000123 without scientific
notation.

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-BOOST">Boost Software License 1.0</a> at your
option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
