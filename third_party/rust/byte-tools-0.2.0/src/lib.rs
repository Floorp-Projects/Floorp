#![no_std]
use core::ptr;

mod read_single;
mod write_single;
mod read_slice;
mod write_slice;

pub use read_single::*;
pub use write_single::*;
pub use read_slice::*;
pub use write_slice::*;

/// Copy bytes from src to dest
#[inline]
pub fn copy_memory(src: &[u8], dst: &mut [u8]) {
    assert!(dst.len() >= src.len());
    unsafe {
        let srcp = src.as_ptr();
        let dstp = dst.as_mut_ptr();
        ptr::copy_nonoverlapping(srcp, dstp, src.len());
    }
}

/// Zero all bytes in dst
#[inline]
pub fn zero(dst: &mut [u8]) {
    set(dst, 0);
}

/// Sets all bytes in `dst` equal to `value`
#[inline]
pub fn set(dst: &mut [u8], value: u8) {
    unsafe {
        ptr::write_bytes(dst.as_mut_ptr(), value, dst.len());
    }
}