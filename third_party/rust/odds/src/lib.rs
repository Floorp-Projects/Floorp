//! Odds and ends — collection miscellania.
//!
//! - Utilities for debug-checked, release-unchecked indexing and slicing
//! - Fixpoint combinator for closures
//! - String and Vec extensions
//!
//! The **odds** crate has the following crate feature flags:
//!
//! - `std`
//!   - Default
//!   - Requires Rust 1.6 *to opt out of*
//!   - Use libstd and std features.
//! - `unstable`.
//!   - Optional.
//!   - Requires nightly channel.
//!   - Implement the closure traits for **Fix**.
//!

#![doc(html_root_url = "https://docs.rs/odds/0.2/")]
#![cfg_attr(feature="unstable", feature(unboxed_closures, fn_traits))]

#![cfg_attr(not(feature="std"), no_std)]

#[cfg(not(feature="std"))]
extern crate core as std;

mod range;
#[path = "fix.rs"]
mod fix_impl;
pub mod char;
pub mod string;
pub mod vec;
pub mod slice;
pub mod stride;

pub use fix_impl::Fix;
pub use fix_impl::fix;
pub use range::IndexRange;

use std::mem;

/// prelude of often used traits and functions
pub mod prelude {
    pub use slice::SliceFind;
    pub use slice::SliceIterExt;
    pub use string::StrExt;
    #[cfg(feature="std")]
    pub use string::StrChunksWindows;
    #[cfg(feature="std")]
    pub use string::StringExt;
    #[cfg(feature="std")]
    pub use vec::{vec, VecExt};
    #[doc(no_inline)]
    pub use IndexRange;
    #[doc(no_inline)]
    pub use fix;
}

/// Compare if **a** and **b** are equal *as pointers*.
#[inline]
pub fn ref_eq<T>(a: &T, b: &T) -> bool {
    ptr_eq(a, b)
}

/// Compare if **a** and **b** are equal pointers.
#[inline]
pub fn ptr_eq<T>(a: *const T, b: *const T) -> bool {
    a == b
}

/// Safe to use with any wholly initialized memory `ptr`
#[inline]
pub unsafe fn raw_byte_repr<T: ?Sized>(ptr: &T) -> &[u8] {
    std::slice::from_raw_parts(
        ptr as *const _ as *const u8,
        mem::size_of_val(ptr),
    )
}

/// Use `debug_assert!` to check indexing in debug mode. In release mode, no checks are done.
#[inline]
pub unsafe fn get_unchecked<T>(data: &[T], index: usize) -> &T {
    debug_assert!(index < data.len());
    data.get_unchecked(index)
}

/// Use `debug_assert!` to check indexing in debug mode. In release mode, no checks are done.
#[inline]
pub unsafe fn get_unchecked_mut<T>(data: &mut [T], index: usize) -> &mut T {
    debug_assert!(index < data.len());
    data.get_unchecked_mut(index)
}

#[inline(always)]
unsafe fn unreachable() -> ! {
    enum Void { }
    match *(1 as *const Void) {
        /* unreachable */
    }
}

/// Act as `debug_assert!` in debug mode, asserting that this point is not reached.
///
/// In release mode, no checks are done, and it acts like the `unreachable` intrinsic.
#[inline(always)]
pub unsafe fn debug_assert_unreachable() -> ! {
    debug_assert!(false, "Entered unreachable section, this is a bug!");
    unreachable()
}

/// Check slicing bounds in debug mode, otherwise just act as an unchecked
/// slice call.
#[inline]
pub unsafe fn slice_unchecked<T>(data: &[T], from: usize, to: usize) -> &[T] {
    debug_assert!((&data[from..to], true).1);
    std::slice::from_raw_parts(data.as_ptr().offset(from as isize), to - from)
}

/// Check slicing bounds in debug mode, otherwise just act as an unchecked
/// slice call.
#[inline]
pub unsafe fn slice_unchecked_mut<T>(data: &mut [T], from: usize, to: usize) -> &mut [T] {
    debug_assert!((&data[from..to], true).1);
    std::slice::from_raw_parts_mut(data.as_mut_ptr().offset(from as isize), to - from)
}

/// Create a length 1 slice out of a reference
pub fn ref_slice<T>(ptr: &T) -> &[T] {
    unsafe {
        std::slice::from_raw_parts(ptr, 1)
    }
}

/// Create a length 1 mutable slice out of a reference
pub fn ref_slice_mut<T>(ptr: &mut T) -> &mut [T] {
    unsafe {
        std::slice::from_raw_parts_mut(ptr, 1)
    }
}


#[test]
fn test_repr() {
    unsafe {
        assert_eq!(raw_byte_repr(&17u8), &[17]);
        assert_eq!(raw_byte_repr("abc"), "abc".as_bytes());
    }
}

#[test]
#[should_panic]
fn test_get_unchecked_1() {
    if cfg!(not(debug_assertions)) {
        panic!();
    }
    unsafe {
        get_unchecked(&[1, 2, 3], 3);
    }
}

#[test]
#[should_panic]
fn test_slice_unchecked_1() {
    // These tests only work in debug mode
    if cfg!(not(debug_assertions)) {
        panic!();
    }
    unsafe {
        slice_unchecked(&[1, 2, 3], 0, 4);
    }
}

#[test]
#[should_panic]
fn test_slice_unchecked_2() {
    if cfg!(not(debug_assertions)) {
        panic!();
    }
    unsafe {
        slice_unchecked(&[1, 2, 3], 4, 4);
    }
}

#[test]
fn test_slice_unchecked_3() {
    // This test only works in release mode
    // test some benign unchecked slicing
    let data = [1, 2, 3, 4];
    let sl = &data[0..0];
    if cfg!(debug_assertions) {
        /* silent */
    } else {
        unsafe {
            assert_eq!(slice_unchecked(sl, 0, 4), &[1, 2, 3, 4]);
        }
    }
}
