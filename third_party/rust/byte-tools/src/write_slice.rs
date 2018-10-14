use core::{ptr, mem};

macro_rules! write_slice {
    ($src:expr, $dst:expr, $ty:ty, $size:expr, $which:ident) => ({
        assert!($size == mem::size_of::<$ty>());
        assert_eq!($dst.len(), $size*$src.len());
        unsafe {
            ptr::copy_nonoverlapping(
                $src.as_ptr() as *const u8,
                $dst.as_mut_ptr(),
                $dst.len());
            let tmp: &mut [$ty] = mem::transmute($dst);
            for v in tmp[..$src.len()].iter_mut() {
                *v = v.$which();
            }
        }
    });
}

/// Write a vector of u32s into a vector of bytes. The values are written in
/// little-endian format.
#[inline]
pub fn write_u32v_le(dst: &mut [u8], src: &[u32]) {
    write_slice!(src, dst, u32, 4, to_le);
}

/// Write a vector of u32s into a vector of bytes. The values are written in
/// big-endian format.
#[inline]
pub fn write_u32v_be(dst: &mut [u8], src: &[u32]) {
    write_slice!(src, dst, u32, 4, to_be);
}

/// Write a vector of u64s into a vector of bytes. The values are written in
/// little-endian format.
#[inline]
pub fn write_u64v_le(dst: &mut [u8], src: &[u64]) {
    write_slice!(src, dst, u64, 8, to_le);
}

/// Write a vector of u64s into a vector of bytes. The values are written in
/// little-endian format.
#[inline]
pub fn write_u64v_be(dst: &mut [u8], src: &[u64]) {
    write_slice!(src, dst, u64, 8, to_be);
}
