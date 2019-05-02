
/// Computes bit reversal by using lookup table to translate a single byte into its reverse.
/// For multi-byte types, the byte order is swapped to complete the reversal.
pub trait LookupReverse<T> {
    /// Swaps the bits such that bit i is now bit N-i, where N is the length of the T in bits.
    fn swap_bits(self) -> T;
}

const REVERSE_LOOKUP: [u8; 256] = [
    0,  128, 64, 192, 32, 160,  96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
    8,  136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
    4,  132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
    12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252, 
    2,  130, 66, 194, 34, 162,  98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
    10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
    6,  134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
    14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
    1,  129, 65, 193, 33, 161,  97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
    9,  137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
    5,  133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
    13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
    3,  131, 67, 195, 35, 163,  99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
    11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
    7,  135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
    15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255
];

impl LookupReverse<u8> for u8 {
    #[inline]
    fn swap_bits(self) -> u8 {
        unsafe { *REVERSE_LOOKUP.get_unchecked(self as usize) }
    }
}

impl LookupReverse<u16> for u16 {
    #[inline]
    fn swap_bits(self) -> u16 {
        unsafe {
            (*REVERSE_LOOKUP.get_unchecked((self & 0xFF) as usize) as u16) << 8 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 8) & 0xFF) as usize) as u16)
        }
    }
}

impl LookupReverse<u32> for u32 {
    #[inline]
    fn swap_bits(self) -> u32 {
        unsafe {
            (*REVERSE_LOOKUP.get_unchecked((self & 0xFF) as usize) as u32) << 24 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 8) & 0xFF) as usize) as u32) << 16 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 16) & 0xFF) as usize) as u32) << 8 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 24) & 0xFF) as usize) as u32)
        }
    }
}

impl LookupReverse<u64> for u64 {
    #[inline]
    fn swap_bits(self) -> u64 {
        unsafe {
            (*REVERSE_LOOKUP.get_unchecked((self & 0xFF) as usize) as u64) << 56 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 8) & 0xFF) as usize) as u64) << 48 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 16) & 0xFF) as usize) as u64) << 40 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 24) & 0xFF) as usize) as u64) << 32 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 32) & 0xFF) as usize) as u64) << 24 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 40) & 0xFF) as usize) as u64) << 16 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 48) & 0xFF) as usize) as u64) << 8 |
            (*REVERSE_LOOKUP.get_unchecked(((self >> 56) & 0xFF) as usize) as u64)
        }
    }
}

impl LookupReverse<usize> for usize {
    #[inline]
    #[cfg(target_pointer_width = "8")]
    fn swap_bits(self) -> usize {
        use LookupReverse;
        LookupReverse::swap_bits(self as u8) as usize
    }

    #[inline]
    #[cfg(target_pointer_width = "16")]
    fn swap_bits(self) -> usize {
        use LookupReverse;
        LookupReverse::swap_bits(self as u16) as usize
    }
    
    #[inline]
    #[cfg(target_pointer_width = "32")]
    fn swap_bits(self) -> usize {
        use LookupReverse;
        LookupReverse::swap_bits(self as u32) as usize
    }

    #[inline]
    #[cfg(target_pointer_width = "64")]
    fn swap_bits(self) -> usize {
        LookupReverse::swap_bits(self as u64) as usize
    }
}

doit_signed!(LookupReverse);
test_suite!();
