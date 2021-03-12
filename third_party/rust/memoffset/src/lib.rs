// Copyright (c) 2017 Gilad Naaman
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//! A crate used for calculating offsets of struct members and their spans.
//!
//! This functionality currently can not be used in compile time code such as `const` or `const fn` definitions.
//!
//! ## Examples
//! ```
//! #[macro_use]
//! extern crate memoffset;
//!
//! #[repr(C, packed)]
//! struct HelpMeIAmTrappedInAStructFactory {
//!     help_me_before_they_: [u8; 15],
//!     a: u32
//! }
//!
//! fn main() {
//!     assert_eq!(offset_of!(HelpMeIAmTrappedInAStructFactory, a), 15);
//!     assert_eq!(span_of!(HelpMeIAmTrappedInAStructFactory, a), 15..19);
//!     assert_eq!(span_of!(HelpMeIAmTrappedInAStructFactory, help_me_before_they_ .. a), 0..15);
//! }
//! ```
//!
//! This functionality can be useful, for example, for checksum calculations:
//!
//! ```ignore
//! #[repr(C, packed)]
//! struct Message {
//!     header: MessageHeader,
//!     fragment_index: u32,
//!     fragment_count: u32,
//!     payload: [u8; 1024],
//!     checksum: u16
//! }
//!
//! let checksum_range = &raw[span_of!(Message, header..checksum)];
//! let checksum = crc16(checksum_range);
//! ```

#![no_std]
#![cfg_attr(
    feature = "unstable_const",
    feature(
        ptr_offset_from,
        const_fn,
        const_ptr_offset_from,
        const_maybe_uninit_as_ptr,
        const_raw_ptr_deref,
    )
)]
#![cfg_attr(feature = "unstable_raw", feature(raw_ref_macros))]

#[macro_use]
#[cfg(doctests)]
#[cfg(doctest)]
extern crate doc_comment;
#[cfg(doctests)]
#[cfg(doctest)]
doctest!("../README.md");

// This `use` statement enables the macros to use `$crate::mem`.
// Doing this enables this crate to function under both std and no-std crates.
#[doc(hidden)]
pub use core::mem;
#[doc(hidden)]
pub use core::ptr;

#[macro_use]
mod raw_field;
#[macro_use]
mod offset_of;
#[macro_use]
mod span_of;
