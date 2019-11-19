//! `tinystr` is a small ASCII-only bounded length string representation.
//!
//! The crate is meant to be used for scenarios where one needs a fast
//! and memory efficient way to store and manipulate short ASCII-only strings.
//!
//! `tinystr` converts each string into an unsigned integer, and uses bitmasking
//! to compare, convert cases and test for common characteristics of strings.
//!
//! # Example
//!
//! ```
//! use tinystr::{TinyStr4, TinyStr8};
//!
//! fn main() {
//!     let s1: TinyStr4 = "tEsT".parse()
//!         .expect("Failed to parse.");
//!
//!     assert_eq!(s1, "tEsT");
//!     assert_eq!(s1.to_ascii_uppercase(), "TEST");
//!     assert_eq!(s1.to_ascii_lowercase(), "test");
//!     assert_eq!(s1.to_ascii_titlecase(), "Test");
//!     assert_eq!(s1.is_ascii_alphanumeric(), true);
//!
//!     let s2: TinyStr8 = "New York".parse()
//!         .expect("Failed to parse.");
//!
//!     assert_eq!(s2, "New York");
//!     assert_eq!(s2.to_ascii_uppercase(), "NEW YORK");
//!     assert_eq!(s2.to_ascii_lowercase(), "new york");
//!     assert_eq!(s2.is_ascii_alphanumeric(), false);
//! }
//! ```
mod helpers;
mod tinystr16;
mod tinystr4;
mod tinystr8;

pub use tinystr16::TinyStr16;
pub use tinystr4::TinyStr4;
pub use tinystr8::TinyStr8;

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
