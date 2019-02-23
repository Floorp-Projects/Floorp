use core::{mem, ptr};

macro_rules! read_single {
    ($src:expr, $size:expr, $ty:ty, $which:ident) => ({
        assert!($size == mem::size_of::<$ty>());
        assert!($size == $src.len());
        unsafe {
            let mut tmp: $ty = mem::uninitialized();
            let p = &mut tmp as *mut _ as *mut u8;
            ptr::copy_nonoverlapping($src.as_ptr(), p, $size);
            tmp.$which()
        }
    });
}

/// Read the value of a vector of bytes as a u32 value in little-endian format.
#[inline]
pub fn read_u32_le(src: &[u8]) -> u32 {
    read_single!(src, 4, u32, to_le)
}

/// Read the value of a vector of bytes as a u32 value in big-endian format.
#[inline]
pub fn read_u32_be(src: &[u8]) -> u32 {
    read_single!(src, 4, u32, to_be)
}

/// Read the value of a vector of bytes as a u64 value in little-endian format.
#[inline]
pub fn read_u64_le(src: &[u8]) -> u64 {
    read_single!(src, 8, u64, to_le)
}

/// Read the value of a vector of bytes as a u64 value in big-endian format.
#[inline]
pub fn read_u64_be(src: &[u8]) -> u64 {
    read_single!(src, 8, u64, to_be)
}
