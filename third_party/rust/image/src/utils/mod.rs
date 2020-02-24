//!  Utilities

use byteorder::{NativeEndian, ByteOrder};
use num_iter::range_step;
use std::mem;
use std::iter::repeat;

#[inline(always)]
pub(crate) fn expand_packed<F>(buf: &mut [u8], channels: usize, bit_depth: u8, mut func: F)
where
    F: FnMut(u8, &mut [u8]),
{
    let pixels = buf.len() / channels * bit_depth as usize;
    let extra = pixels % 8;
    let entries = pixels / 8 + match extra {
        0 => 0,
        _ => 1,
    };
    let mask = ((1u16 << bit_depth) - 1) as u8;
    let i = (0..entries)
        .rev() // Reverse iterator
        .flat_map(|idx|
            // This has to be reversed to
            range_step(0, 8, bit_depth)
            .zip(repeat(idx))
        )
        .skip(extra);
    let channels = channels as isize;
    let j = range_step(buf.len() as isize - channels, -channels, -channels);
    //let j = range_step(0, buf.len(), channels).rev(); // ideal solution;
    for ((shift, i), j) in i.zip(j) {
        let pixel = (buf[i] & (mask << shift)) >> shift;
        func(pixel, &mut buf[j as usize..(j + channels) as usize])
    }
}

/// Expand a buffer of packed 1, 2, or 4 bits integers into u8's. Assumes that
/// every `row_size` entries there are padding bits up to the next byte boundry.
pub(crate) fn expand_bits(bit_depth: u8, row_size: u32, buf: &[u8]) -> Vec<u8> {
    // Note: this conversion assumes that the scanlines begin on byte boundaries
    let mask = (1u8 << bit_depth as usize) - 1;
    let scaling_factor = 255 / ((1 << bit_depth as usize) - 1);
    let bit_width = row_size * u32::from(bit_depth);
    let skip = if bit_width % 8 == 0 {
        0
    } else {
        (8 - bit_width % 8) / u32::from(bit_depth)
    };
    let row_len = row_size + skip;
    let mut p = Vec::new();
    let mut i = 0;
    for v in buf {
        for shift in num_iter::range_step_inclusive(8i8-(bit_depth as i8), 0, -(bit_depth as i8)) {
            // skip the pixels that can be neglected because scanlines should
            // start at byte boundaries
            if i % (row_len as usize) < (row_size as usize) {
                let pixel = (v & mask << shift as usize) >> shift as usize;
                p.push(pixel * scaling_factor);
            }
            i += 1;
        }
    }
    p
}

pub(crate) fn vec_u16_into_u8(vec: Vec<u16>) -> Vec<u8> {
    // Do this way until we find a way to not alloc/dealloc but get llvm to realloc instead.
    vec_u16_copy_u8(&vec)
}

pub(crate) fn vec_u16_copy_u8(vec: &[u16]) -> Vec<u8> {
    let mut new = vec![0; vec.len() * mem::size_of::<u16>()];
    NativeEndian::write_u16_into(&vec[..], &mut new[..]);
    new
}


/// A marker struct for __NonExhaustive enums.
///
/// This is an empty type that can not be constructed. When an enum contains a tuple variant that
/// includes this type the optimizer can statically determined tha the branch is never taken while
/// at the same time the matching of the branch is required.
///
/// The effect is thus very similar to the actual `#[non_exhaustive]` attribute with no runtime
/// costs. Also note that we use a dirty trick to not only hide this type from the doc but make it
/// inaccessible. The visibility in this module is pub but the module itself is not and the
/// top-level crate never exports the type.
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct NonExhaustiveMarker {
    /// Allows this crate, and this crate only, to match on the impossibility of this variant.
    pub(crate) _private: Empty,
}

#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) enum Empty { }

#[cfg(test)]
mod test {
    #[test]
    fn gray_to_luma8_skip() {
        let check = |bit_depth, w, from, to| {
            assert_eq!(
                super::expand_bits(bit_depth, w, from),
                to);
        };
        // Bit depth 1, skip is more than half a byte
        check(
            1, 10,
            &[0b11110000, 0b11000000, 0b00001111, 0b11000000],
            vec![255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255]);
        // Bit depth 2, skip is more than half a byte
        check(
            2, 5,
            &[0b11110000, 0b11000000, 0b00001111, 0b11000000],
            vec![255, 255, 0, 0, 255, 0, 0, 255, 255, 255]);
        // Bit depth 2, skip is 0
        check(
            2, 4,
            &[0b11110000, 0b00001111],
            vec![255, 255, 0, 0, 0, 0, 255, 255]);
        // Bit depth 4, skip is half a byte
        check(
            4, 1,
            &[0b11110011, 0b00001100],
            vec![255, 0]);
    }
}
