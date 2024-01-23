//! Utilities to help with buffering.

#![allow(unsafe_code)]

use core::mem::MaybeUninit;
use core::slice;

/// Split an uninitialized byte slice into initialized and uninitialized parts.
///
/// # Safety
///
/// At least `init` bytes must be initialized.
#[inline]
pub(super) unsafe fn split_init(
    buf: &mut [MaybeUninit<u8>],
    init: usize,
) -> (&mut [u8], &mut [MaybeUninit<u8>]) {
    let (init, uninit) = buf.split_at_mut(init);
    let init = slice::from_raw_parts_mut(init.as_mut_ptr() as *mut u8, init.len());
    (init, uninit)
}
