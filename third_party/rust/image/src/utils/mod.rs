//!  Utilities

use byteorder::{NativeEndian, ByteOrder};
use num_iter::range_step;
use std::mem;
use std::iter::repeat;

#[inline(always)]
pub fn expand_packed<F>(buf: &mut [u8], channels: usize, bit_depth: u8, mut func: F)
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

pub fn vec_u16_into_u8(vec: Vec<u16>) -> Vec<u8> {
    // Do this way until we find a way to not alloc/dealloc but get llvm to realloc instead.
    vec_u16_copy_u8(&vec)
}

pub fn vec_u16_copy_u8(vec: &Vec<u16>) -> Vec<u8> {
    let mut new = vec![0; vec.len() * mem::size_of::<u16>()];
    NativeEndian::write_u16_into(&vec[..], &mut new[..]);
    new
}
