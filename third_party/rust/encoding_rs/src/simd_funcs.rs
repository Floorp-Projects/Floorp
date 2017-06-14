// Copyright 2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use simd::u8x16;
use simd::i8x16;
use simd::u16x8;
use simd::i16x8;
use simd::Simd;

extern "platform-intrinsic" {
    fn simd_shuffle16<T: Simd, U: Simd<Elem = T::Elem>>(x: T, y: T, idx: [u32; 16]) -> U;
    fn x86_mm_packus_epi16(x: i16x8, y: i16x8) -> u8x16;
    fn x86_mm_movemask_epi8(x: i8x16) -> i32;
}

// TODO: Migrate unaligned access to stdlib code if/when the RFC
// https://github.com/rust-lang/rfcs/pull/1725 is implemented.

#[inline(always)]
pub unsafe fn load16_unaligned(ptr: *const u8) -> u8x16 {
    let mut simd = ::std::mem::uninitialized();
    ::std::ptr::copy_nonoverlapping(ptr, &mut simd as *mut u8x16 as *mut u8, 16);
    simd
}

#[inline(always)]
pub unsafe fn load16_aligned(ptr: *const u8) -> u8x16 {
    *(ptr as *const u8x16)
}

#[inline(always)]
pub unsafe fn store16_unaligned(ptr: *mut u8, s: u8x16) {
    ::std::ptr::copy_nonoverlapping(&s as *const u8x16 as *const u8, ptr, 16);
}

#[inline(always)]
pub unsafe fn store16_aligned(ptr: *mut u8, s: u8x16) {
    *(ptr as *mut u8x16) = s;
}

#[inline(always)]
pub unsafe fn load8_unaligned(ptr: *const u16) -> u16x8 {
    let mut simd = ::std::mem::uninitialized();
    ::std::ptr::copy_nonoverlapping(ptr as *const u8, &mut simd as *mut u16x8 as *mut u8, 16);
    simd
}

#[inline(always)]
pub unsafe fn load8_aligned(ptr: *const u16) -> u16x8 {
    *(ptr as *const u16x8)
}

#[inline(always)]
pub unsafe fn store8_unaligned(ptr: *mut u16, s: u16x8) {
    ::std::ptr::copy_nonoverlapping(&s as *const u16x8 as *const u8, ptr as *mut u8, 16);
}

#[inline(always)]
pub unsafe fn store8_aligned(ptr: *mut u16, s: u16x8) {
    *(ptr as *mut u16x8) = s;
}

/// _mm_movemask_epi8 in SSE2. vec_all_lt in AltiVec.
#[inline(always)]
pub fn is_ascii(s: u8x16) -> bool {
    unsafe {
        let signed: i8x16 = ::std::mem::transmute_copy(&s);
        x86_mm_movemask_epi8(signed) == 0
    }
}


/// _mm_movemask_epi8 in SSE2.
#[inline(always)]
pub fn check_ascii(s: u8x16) -> Option<usize> {
    let mask = unsafe {
        let signed: i8x16 = ::std::mem::transmute_copy(&s);
        x86_mm_movemask_epi8(signed)
    };
    if mask == 0 {
        return None;
    }
    // We don't extract the non-ascii byte from the SIMD register, because
    // at least on Haswell, it seems faster to let the caller re-read it from
    // memory.
    Some(mask.trailing_zeros() as usize)
}

/// vzipq_u8 in NEON. _mm_unpacklo_epi8 and
/// _mm_unpackhi_epi8 in SSE2. vec_mergeh and vec_mergel or vec_unpackh and
/// vec_unpackl in AltiVec.
#[inline(always)]
pub fn unpack(s: u8x16) -> (u16x8, u16x8) {
    unsafe {
        let first: u8x16 = simd_shuffle16(
            s,
            u8x16::splat(0),
            [0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23],
        );
        let second: u8x16 = simd_shuffle16(
            s,
            u8x16::splat(0),
            [8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31],
        );
        (::std::mem::transmute_copy(&first), ::std::mem::transmute_copy(&second))
    }
}

/// vuzpq_u8 in NEON. _mm_packus_epi16 in SSE2. vec_packsu *followed* by ASCII
/// check in AltiVec.
#[inline(always)]
pub unsafe fn pack_basic_latin(a: u16x8, b: u16x8) -> Option<u8x16> {
    // If the 16-bit lane is out of range positive, the 8-bit lane becomes 0xFF
    // when packing, which would allow us to pack later and then check for
    // ASCII, but if the 16-bit lane is negative, the 8-bit lane becomes 0x00.
    // Sigh. Hence, check first.
    let above_ascii = u16x8::splat(0x80);
    if a.lt(above_ascii).all() && b.lt(above_ascii).all() {
        let first: i16x8 = ::std::mem::transmute_copy(&a);
        let second: i16x8 = ::std::mem::transmute_copy(&b);
        Some(x86_mm_packus_epi16(first, second))
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_unpack() {
        let ascii: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let basic_latin: [u16; 16] = [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70,
                                      0x71, 0x72, 0x73, 0x74, 0x75, 0x76];
        let simd = unsafe { load16_unaligned(ascii.as_ptr()) };
        let mut vec = Vec::with_capacity(16);
        vec.resize(16, 0u16);
        let (first, second) = unpack(simd);
        let ptr = vec.as_mut_ptr();
        unsafe {
            store8_unaligned(ptr, first);
            store8_unaligned(ptr.offset(8), second);
        }
        assert_eq!(&vec[..], &basic_latin[..]);
    }

    #[test]
    fn test_pack_basic_latin_success() {
        let ascii: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let basic_latin: [u16; 16] = [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70,
                                      0x71, 0x72, 0x73, 0x74, 0x75, 0x76];
        let first = unsafe { load8_unaligned(basic_latin.as_ptr()) };
        let second = unsafe { load8_unaligned(basic_latin.as_ptr().offset(8)) };
        let mut vec = Vec::with_capacity(16);
        vec.resize(16, 0u8);
        let ptr = vec.as_mut_ptr();
        unsafe {
            let packed = pack_basic_latin(first, second).unwrap();
            store16_unaligned(ptr, packed);
        }
        assert_eq!(&vec[..], &ascii[..]);
    }

    #[test]
    fn test_pack_basic_latin_c0() {
        let input: [u16; 16] = [0x61, 0x62, 0x63, 0x81, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                                0x72, 0x73, 0x74, 0x75, 0x76];
        let first = unsafe { load8_unaligned(input.as_ptr()) };
        let second = unsafe { load8_unaligned(input.as_ptr().offset(8)) };
        unsafe {
            assert!(pack_basic_latin(first, second).is_none());
        }
    }

    #[test]
    fn test_pack_basic_latin_0fff() {
        let input: [u16; 16] = [0x61, 0x62, 0x63, 0x0FFF, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70,
                                0x71, 0x72, 0x73, 0x74, 0x75, 0x76];
        let first = unsafe { load8_unaligned(input.as_ptr()) };
        let second = unsafe { load8_unaligned(input.as_ptr().offset(8)) };
        unsafe {
            assert!(pack_basic_latin(first, second).is_none());
        }
    }

    #[test]
    fn test_pack_basic_latin_ffff() {
        let input: [u16; 16] = [0x61, 0x62, 0x63, 0xFFFF, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70,
                                0x71, 0x72, 0x73, 0x74, 0x75, 0x76];
        let first = unsafe { load8_unaligned(input.as_ptr()) };
        let second = unsafe { load8_unaligned(input.as_ptr().offset(8)) };
        unsafe {
            assert!(pack_basic_latin(first, second).is_none());
        }
    }

    #[test]
    fn test_is_ascii_success() {
        let ascii: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let simd = unsafe { load16_unaligned(ascii.as_ptr()) };
        assert!(is_ascii(simd));
    }

    #[test]
    fn test_is_ascii_failure() {
        let input: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x81, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let simd = unsafe { load16_unaligned(input.as_ptr()) };
        assert!(!is_ascii(simd));
    }

    #[test]
    fn test_check_ascii() {
        let input: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x81, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let simd = unsafe { load16_unaligned(input.as_ptr()) };
        match check_ascii(simd) {
            None => unreachable!(),
            Some(consumed) => {
                assert_eq!(consumed, 4);
            }
        }
    }

    #[test]
    fn test_alu() {
        let input: [u8; 16] = [0x61, 0x62, 0x63, 0x64, 0x81, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
                               0x72, 0x73, 0x74, 0x75, 0x76];
        let mut alu = 0u64;
        unsafe {
            ::std::ptr::copy_nonoverlapping(input.as_ptr(), &mut alu as *mut u64 as *mut u8, 8);
        }
        let masked = alu & 0x8080808080808080;
        assert_eq!(masked.trailing_zeros(), 39);
    }

}
