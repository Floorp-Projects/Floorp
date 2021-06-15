//! `tinystr` is a small ASCII-only bounded length string representation.
//!
//! The crate is meant to be used for scenarios where one needs a fast
//! and memory efficient way to store and manipulate short ASCII-only strings.
//!
//! `tinystr` converts each string into an unsigned integer, and uses bitmasking
//! to compare, convert cases and test for common characteristics of strings.
//! 
//! # Details
//!
//! The crate provides three structs and an enum:
//! * `TinyStr4` an ASCII-only string limited to 4 characters.
//! * `TinyStr8` an ASCII-only string limited to 8 characters.
//! * `TinyStr16` an ASCII-only string limited to 16 characters.
//! * `TinyStrAuto` (enum):
//!   * `Tiny` when the string is 16 characters or less.
//!   * `Heap` when the string is 17 or more characters.
//!
//! `TinyStrAuto` stores the string as a TinyStr16 when it is short enough, or else falls back to a
//! standard `String`. You should use TinyStrAuto when you expect most strings to be 16 characters
//! or smaller, but occasionally you receive one that exceeds that length. Unlike the structs,
//! `TinyStrAuto` does not implement `Copy`.
//!
//! # no_std
//!
//! Disable the `std` feature of this crate to make it `#[no_std]`. Doing so disables `TinyStrAuto`.
//! You can re-enable `TinyStrAuto` in `#[no_std]` mode by enabling the `alloc` feature.
//!
//! # Example
//!
//! ```
//! use tinystr::{TinyStr4, TinyStr8, TinyStr16, TinyStrAuto};
//!
//! let s1: TinyStr4 = "tEsT".parse()
//!     .expect("Failed to parse.");
//!
//! assert_eq!(s1, "tEsT");
//! assert_eq!(s1.to_ascii_uppercase(), "TEST");
//! assert_eq!(s1.to_ascii_lowercase(), "test");
//! assert_eq!(s1.to_ascii_titlecase(), "Test");
//! assert_eq!(s1.is_ascii_alphanumeric(), true);
//!
//! let s2: TinyStr8 = "New York".parse()
//!      .expect("Failed to parse.");
//!
//! assert_eq!(s2, "New York");
//! assert_eq!(s2.to_ascii_uppercase(), "NEW YORK");
//! assert_eq!(s2.to_ascii_lowercase(), "new york");
//! assert_eq!(s2.to_ascii_titlecase(), "New york");
//! assert_eq!(s2.is_ascii_alphanumeric(), false);
//!
//! let s3: TinyStr16 = "metaMoRphosis123".parse()
//!     .expect("Failed to parse.");
//!
//! assert_eq!(s3, "metaMoRphosis123");
//! assert_eq!(s3.to_ascii_uppercase(), "METAMORPHOSIS123");
//! assert_eq!(s3.to_ascii_lowercase(), "metamorphosis123");
//! assert_eq!(s3.to_ascii_titlecase(), "Metamorphosis123");
//! assert_eq!(s3.is_ascii_alphanumeric(), true);
//!
//! let s4: TinyStrAuto = "shortNoAlloc".parse().unwrap();
//! assert!(matches!(s4, TinyStrAuto::Tiny { .. }));
//! assert_eq!(s4, "shortNoAlloc");
//!
//! let s5: TinyStrAuto = "longFallbackToHeap".parse().unwrap();
//! assert!(matches!(s5, TinyStrAuto::Heap { .. }));
//! assert_eq!(s5, "longFallbackToHeap");
//! ```

#![no_std]

#[cfg(any(feature = "std", test))]
extern crate std;

#[cfg(all(not(feature = "std"), not(test)))]
extern crate core as std;

mod helpers;
mod tinystr16;
mod tinystr4;
mod tinystr8;

#[cfg(any(feature = "std", feature = "alloc"))]
mod tinystrauto;

pub use tinystr16::TinyStr16;
pub use tinystr4::TinyStr4;
pub use tinystr8::TinyStr8;

#[cfg(any(feature = "std", feature = "alloc"))]
pub use tinystrauto::TinyStrAuto;

#[cfg(feature = "macros")]
pub use tinystr_macros as macros;

/// Enum to store the various types of errors that can cause parsing a TinyStr to fail.
#[derive(PartialEq, Eq, Debug)]
pub enum Error {
    /// String is too large or too small to store as TinyStr.
    InvalidSize,
    /// String is empty.
    InvalidNull,
    /// String contains non-ASCII character(s).
    NonAscii,
}
