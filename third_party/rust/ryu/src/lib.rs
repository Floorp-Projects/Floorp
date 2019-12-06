//! Pure Rust implementation of Ryū, an algorithm to quickly convert floating
//! point numbers to decimal strings.
//!
//! The PLDI'18 paper [*Ryū: fast float-to-string conversion*][paper] by Ulf
//! Adams includes a complete correctness proof of the algorithm. The paper is
//! available under the creative commons CC-BY-SA license.
//!
//! This Rust implementation is a line-by-line port of Ulf Adams' implementation
//! in C, [https://github.com/ulfjack/ryu][upstream]. The [`ryu::raw`][raw]
//! module exposes exactly the API and formatting of the C implementation as
//! unsafe pure Rust functions. There is additionally a safe API as demonstrated
//! in the example code below. The safe API uses the same underlying Ryū
//! algorithm but diverges from the formatting of the C implementation to
//! produce more human-readable output, for example `0.3` rather than `3E-1`.
//!
//! [paper]: https://dl.acm.org/citation.cfm?id=3192369
//! [upstream]: https://github.com/ulfjack/ryu
//! [raw]: raw/index.html
//!
//! # Examples
//!
//! ```rust
//! extern crate ryu;
//!
//! fn main() {
//!     let mut buffer = ryu::Buffer::new();
//!     let printed = buffer.format(1.234);
//!     assert_eq!(printed, "1.234");
//! }
//! ```

#![no_std]
#![doc(html_root_url = "https://docs.rs/ryu/0.2.4")]
#![cfg_attr(feature = "no-panic", feature(use_extern_macros))]
#![cfg_attr(
    feature = "cargo-clippy",
    allow(
        cast_lossless,
        cyclomatic_complexity,
        many_single_char_names,
        needless_pass_by_value,
        unreadable_literal,
    )
)]

#[cfg(feature = "no-panic")]
extern crate no_panic;

mod buffer;
mod common;
mod d2s;
#[cfg(not(feature = "small"))]
mod d2s_full_table;
#[cfg(feature = "small")]
mod d2s_small_table;
mod digit_table;
mod f2s;
#[cfg(not(integer128))]
mod mulshift128;
mod pretty;

pub use buffer::{Buffer, Float};

/// Unsafe functions that exactly mirror the API of the C implementation of Ryū.
pub mod raw {
    pub use d2s::d2s_buffered_n;
    pub use f2s::f2s_buffered_n;
}
