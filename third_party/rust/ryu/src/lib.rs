//! [![github]](https://github.com/dtolnay/ryu)&ensp;[![crates-io]](https://crates.io/crates/ryu)&ensp;[![docs-rs]](https://docs.rs/ryu)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! <br>
//!
//! Pure Rust implementation of Ryū, an algorithm to quickly convert floating
//! point numbers to decimal strings.
//!
//! The PLDI'18 paper [*Ryū: fast float-to-string conversion*][paper] by Ulf
//! Adams includes a complete correctness proof of the algorithm. The paper is
//! available under the creative commons CC-BY-SA license.
//!
//! This Rust implementation is a line-by-line port of Ulf Adams' implementation
//! in C, [https://github.com/ulfjack/ryu][upstream].
//!
//! [paper]: https://dl.acm.org/citation.cfm?id=3192369
//! [upstream]: https://github.com/ulfjack/ryu
//!
//! # Example
//!
//! ```
//! fn main() {
//!     let mut buffer = ryu::Buffer::new();
//!     let printed = buffer.format(1.234);
//!     assert_eq!(printed, "1.234");
//! }
//! ```
//!
//! ## Performance
//!
//! You can run upstream's benchmarks with:
//!
//! ```console
//! $ git clone https://github.com/ulfjack/ryu c-ryu
//! $ cd c-ryu
//! $ bazel run -c opt //ryu/benchmark
//! ```
//!
//! And the same benchmark against our implementation with:
//!
//! ```console
//! $ git clone https://github.com/dtolnay/ryu rust-ryu
//! $ cd rust-ryu
//! $ cargo run --example upstream_benchmark --release
//! ```
//!
//! These benchmarks measure the average time to print a 32-bit float and average
//! time to print a 64-bit float, where the inputs are distributed as uniform random
//! bit patterns 32 and 64 bits wide.
//!
//! The upstream C code, the unsafe direct Rust port, and the safe pretty Rust API
//! all perform the same, taking around 21 nanoseconds to format a 32-bit float and
//! 31 nanoseconds to format a 64-bit float.
//!
//! There is also a Rust-specific benchmark comparing this implementation to the
//! standard library which you can run with:
//!
//! ```console
//! $ cargo bench
//! ```
//!
//! The benchmark shows Ryū approximately 4-10x faster than the standard library
//! across a range of f32 and f64 inputs. Measurements are in nanoseconds per
//! iteration; smaller is better.
//!
//! | type=f32 | 0.0  | 0.1234 | 2.718281828459045 | f32::MAX |
//! |:--------:|:----:|:------:|:-----------------:|:--------:|
//! | RYU      | 3ns  | 28ns   | 23ns              | 22ns     |
//! | STD      | 40ns | 106ns  | 128ns             | 110ns    |
//!
//! | type=f64 | 0.0  | 0.1234 | 2.718281828459045 | f64::MAX |
//! |:--------:|:----:|:------:|:-----------------:|:--------:|
//! | RYU      | 3ns  | 50ns   | 35ns              | 32ns     |
//! | STD      | 39ns | 105ns  | 128ns             | 202ns    |
//!
//! ## Formatting
//!
//! This library tends to produce more human-readable output than the standard
//! library's to\_string, which never uses scientific notation. Here are two
//! examples:
//!
//! - *ryu:* 1.23e40, *std:* 12300000000000000000000000000000000000000
//! - *ryu:* 1.23e-40, *std:* 0.000000000000000000000000000000000000000123
//!
//! Both libraries print short decimals such as 0.0000123 without scientific
//! notation.

#![no_std]
#![doc(html_root_url = "https://docs.rs/ryu/1.0.5")]
#![cfg_attr(feature = "cargo-clippy", allow(renamed_and_removed_lints))]
#![cfg_attr(
    feature = "cargo-clippy",
    allow(cast_lossless, many_single_char_names, unreadable_literal,)
)]

mod buffer;
mod common;
mod d2s;
#[cfg(not(feature = "small"))]
mod d2s_full_table;
mod d2s_intrinsics;
#[cfg(feature = "small")]
mod d2s_small_table;
mod digit_table;
mod f2s;
mod f2s_intrinsics;
mod pretty;

pub use crate::buffer::{Buffer, Float};

/// Unsafe functions that mirror the API of the C implementation of Ryū.
pub mod raw {
    pub use crate::pretty::{format32, format64};
}
