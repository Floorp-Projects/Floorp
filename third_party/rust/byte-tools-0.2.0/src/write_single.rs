use core::{mem, ptr};

macro_rules! write_single {
    ($dst:expr, $n:expr, $size:expr, $which:ident) => ({
        assert!($size == $dst.len());
        unsafe {
            let bytes = mem::transmute::<_, [u8; $size]>($n.$which());
            ptr::copy_nonoverlapping((&bytes).as_ptr(), $dst.as_mut_ptr(), $size);
        }
    });
}

/// Write a u32 into a vector, which must be 4 bytes long. The value is written
/// in little-endian format.
#[inline]
pub fn write_u32_le(dst: &mut [u8], n: u32) {
    write_single!(dst, n, 4, to_le);
}

/// Write a u32 into a vector, which must be 4 bytes long. The value is written
/// in big-endian format.
#[inline]
pub fn write_u32_be(dst: &mut [u8], n: u32) {
    write_single!(dst, n, 4, to_be);
}

/// Write a u64 into a vector, which must be 8 bytes long. The value is written
/// in little-endian format.
#[inline]
pub fn write_u64_le(dst: &mut [u8], n: u64) {
    write_single!(dst, n, 8, to_le);
}

/// Write a u64 into a vector, which must be 8 bytes long. The value is written
/// in big-endian format.
#[inline]
pub fn write_u64_be(dst: &mut [u8], n: u64) {
    write_single!(dst, n, 8, to_be);
}
