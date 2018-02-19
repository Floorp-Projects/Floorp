use adler32::RollingAdler32;
use libc::{c_uint, c_ulong};

pub const MZ_ADLER32_INIT: c_ulong = 1;

pub const HUFFMAN_LENGTH_ORDER: [u8; 19] =
    [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

pub fn update_adler32(adler: c_uint, data: &[u8]) -> c_uint {
    let mut hash = RollingAdler32::from_value(adler);
    hash.update_buffer(data);
    hash.hash()
}
