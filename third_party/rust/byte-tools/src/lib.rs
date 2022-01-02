#![no_std]
use core::ptr;

/// Copy bytes from `src` to `dst`
///
/// Panics if `src.len() < dst.len()`
#[inline(always)]
pub fn copy(src: &[u8], dst: &mut [u8]) {
    assert!(dst.len() >= src.len());
    unsafe {
        ptr::copy_nonoverlapping(src.as_ptr(), dst.as_mut_ptr(), src.len());
    }
}

/// Zero all bytes in `dst`
#[inline(always)]
pub fn zero(dst: &mut [u8]) {
    unsafe {
        ptr::write_bytes(dst.as_mut_ptr(), 0, dst.len());
    }
}

/// Sets all bytes in `dst` equal to `value`
#[inline(always)]
pub fn set(dst: &mut [u8], value: u8) {
    unsafe {
        ptr::write_bytes(dst.as_mut_ptr(), value, dst.len());
    }
}
