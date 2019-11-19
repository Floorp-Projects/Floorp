use std::num::NonZeroU32;
use std::ptr::copy_nonoverlapping;

use super::Error;

#[inline(always)]
pub(crate) unsafe fn make_4byte_bytes(
    bytes: &[u8],
    len: usize,
    mask: u32,
) -> Result<NonZeroU32, Error> {
    // Mask is always supplied as little-endian.
    let mask = u32::from_le(mask);
    let mut word: u32 = 0;
    copy_nonoverlapping(bytes.as_ptr(), &mut word as *mut u32 as *mut u8, len);
    if (word & mask) != 0 {
        return Err(Error::NonAscii);
    }
    if ((mask - word) & mask) != 0 {
        return Err(Error::InvalidNull);
    }
    Ok(NonZeroU32::new_unchecked(word))
}
