use core::ptr;

macro_rules! read_slice {
    ($src:expr, $dst:expr, $size:expr, $which:ident) => ({
        assert_eq!($size*$dst.len(), $src.len());
        unsafe {
            ptr::copy_nonoverlapping(
                $src.as_ptr(),
                $dst.as_mut_ptr() as *mut u8,
                $src.len());
        }
        for v in $dst.iter_mut() {
            *v = v.$which();
        }
    });
}

/// Read a vector of bytes into a vector of u32s. The values are read in
/// little-endian format.
#[inline]
pub fn read_u32v_le(dst: &mut [u32], src: &[u8]) {
    read_slice!(src, dst, 4, to_le);
}

/// Read a vector of bytes into a vector of u32s. The values are read in
/// big-endian format.
#[inline]
pub fn read_u32v_be(dst: &mut [u32], src: &[u8]) {
    read_slice!(src, dst, 4, to_be);
}

/// Read a vector of bytes into a vector of u64s. The values are read in
/// little-endian format.
#[inline]
pub fn read_u64v_le(dst: &mut [u64], src: &[u8]) {
    read_slice!(src, dst, 8, to_le);
}

/// Read a vector of bytes into a vector of u64s. The values are read in
/// big-endian format.
#[inline]
pub fn read_u64v_be(dst: &mut [u64], src: &[u8]) {
    read_slice!(src, dst, 8, to_be);
}
