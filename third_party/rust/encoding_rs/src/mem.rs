// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Functions for converting between different in-RAM representations of text
//! and for quickly checking if the Unicode Bidirectional Algorithm can be
//! avoided.
//!
//! By using slices for output, the functions here seek to enable by-register
//! (ALU register or SIMD register as available) operations in order to
//! outperform iterator-based conversions available in the Rust standard
//! library.
//!
//! _Note:_ "Latin1" in this module refers to the Unicode range from U+0000 to
//! U+00FF, inclusive, and does not refer to the windows-1252 range. This
//! in-memory encoding is sometimes used as a storage optimization of text
//! when UTF-16 indexing and length semantics are exposed.

use super::in_inclusive_range16;
use super::in_inclusive_range32;
use super::in_inclusive_range8;
use super::in_range16;
use super::in_range32;
use super::DecoderResult;
use super::EncoderResult;
use ascii::*;
use utf_8::*;

cfg_if!{
    if #[cfg(feature = "simd-accel")] {
        use ::std::intrinsics::unlikely;
    } else {
        #[inline(always)]
        // Unsafe to match the intrinsic, which is needlessly unsafe.
        unsafe fn unlikely(b: bool) -> bool {
            b
        }
    }
}

/// Classification of text as Latin1 (all code points are below U+0100),
/// left-to-right with some non-Latin1 characters or as containing at least
/// some right-to-left characters.
#[must_use]
#[derive(Debug, PartialEq, Eq)]
#[repr(C)]
pub enum Latin1Bidi {
    /// Every character is below U+0100.
    Latin1 = 0,
    /// There is at least one character that's U+0100 or higher, but there
    /// are no right-to-left characters.
    LeftToRight = 1,
    /// There is at least one right-to-left character.
    Bidi = 2,
}

// `as` truncates, so works on 32-bit, too.
#[allow(dead_code)]
const LATIN1_MASK: usize = 0xFF00FF00_FF00FF00u64 as usize;

#[allow(unused_macros)]
macro_rules! by_unit_check_alu {
    ($name:ident, $unit:ty, $bound:expr, $mask:ident) => {
        #[inline(always)]
        fn $name(buffer: &[$unit]) -> bool {
            let mut offset = 0usize;
            let mut accu = 0usize;
            let unit_size = ::std::mem::size_of::<$unit>();
            let len = buffer.len();
            if len >= ALU_ALIGNMENT / unit_size {
                // The most common reason to return `false` is for the first code
                // unit to fail the test, so check that first.
                if buffer[0] >= $bound {
                    return false;
                }
                let src = buffer.as_ptr();
                let mut until_alignment = ((ALU_ALIGNMENT - ((src as usize) & ALU_ALIGNMENT_MASK))
                    & ALU_ALIGNMENT_MASK) / unit_size;
                if until_alignment + ALU_ALIGNMENT / unit_size <= len {
                    if until_alignment != 0 {
                        accu |= buffer[offset] as usize;
                        offset += 1;
                        until_alignment -= 1;
                        while until_alignment != 0 {
                            accu |= buffer[offset] as usize;
                            offset += 1;
                            until_alignment -= 1;
                        }
                        if accu >= $bound {
                            return false;
                        }
                    }
                    let len_minus_stride = len - ALU_ALIGNMENT / unit_size;
                    if offset + (4 * (ALU_ALIGNMENT / unit_size)) <= len {
                        let len_minus_unroll = len - (4 * (ALU_ALIGNMENT / unit_size));
                        loop {
                            let unroll_accu = unsafe {
                                *(src.offset(offset as isize) as *const usize)
                            } | unsafe {
                                *(src.offset((offset + (ALU_ALIGNMENT / unit_size)) as isize)
                                    as *const usize)
                            } | unsafe {
                                *(src.offset((offset + (2 * (ALU_ALIGNMENT / unit_size))) as isize)
                                    as *const usize)
                            } | unsafe {
                                *(src.offset((offset + (3 * (ALU_ALIGNMENT / unit_size))) as isize)
                                    as *const usize)
                            };
                            if unroll_accu & $mask != 0 {
                                return false;
                            }
                            offset += 4 * (ALU_ALIGNMENT / unit_size);
                            if offset > len_minus_unroll {
                                break;
                            }
                        }
                    }
                    while offset <= len_minus_stride {
                        accu |= unsafe { *(src.offset(offset as isize) as *const usize) };
                        offset += ALU_ALIGNMENT / unit_size;
                    }
                }
            }
            for &unit in &buffer[offset..] {
                accu |= unit as usize;
            }
            accu & $mask == 0
        }
    };
}

#[allow(unused_macros)]
macro_rules! by_unit_check_simd {
    ($name:ident, $unit:ty, $splat:expr, $simd_ty:ty, $bound:expr, $func:ident) => {
        #[inline(always)]
        fn $name(buffer: &[$unit]) -> bool {
            let mut offset = 0usize;
            let mut accu = 0usize;
            let unit_size = ::std::mem::size_of::<$unit>();
            let len = buffer.len();
            if len >= SIMD_STRIDE_SIZE / unit_size {
                // The most common reason to return `false` is for the first code
                // unit to fail the test, so check that first.
                if buffer[0] >= $bound {
                    return false;
                }
                let src = buffer.as_ptr();
                let mut until_alignment = ((SIMD_ALIGNMENT - ((src as usize) & SIMD_ALIGNMENT_MASK))
                    & SIMD_ALIGNMENT_MASK) / unit_size;
                if until_alignment + SIMD_STRIDE_SIZE / unit_size <= len {
                    if until_alignment != 0 {
                        accu |= buffer[offset] as usize;
                        offset += 1;
                        until_alignment -= 1;
                        while until_alignment != 0 {
                            accu |= buffer[offset] as usize;
                            offset += 1;
                            until_alignment -= 1;
                        }
                        if accu >= $bound {
                            return false;
                        }
                    }
                    let len_minus_stride = len - SIMD_STRIDE_SIZE / unit_size;
                    if offset + (4 * (SIMD_STRIDE_SIZE / unit_size)) <= len {
                        let len_minus_unroll = len - (4 * (SIMD_STRIDE_SIZE / unit_size));
                        loop {
                            let unroll_accu = unsafe {
                                *(src.offset(offset as isize) as *const $simd_ty)
                            } | unsafe {
                                *(src.offset((offset + (SIMD_STRIDE_SIZE / unit_size)) as isize)
                                    as *const $simd_ty)
                            } | unsafe {
                                *(src.offset(
                                    (offset + (2 * (SIMD_STRIDE_SIZE / unit_size))) as isize,
                                ) as *const $simd_ty)
                            } | unsafe {
                                *(src.offset(
                                    (offset + (3 * (SIMD_STRIDE_SIZE / unit_size))) as isize,
                                ) as *const $simd_ty)
                            };
                            if !$func(unroll_accu) {
                                return false;
                            }
                            offset += 4 * (SIMD_STRIDE_SIZE / unit_size);
                            if offset > len_minus_unroll {
                                break;
                            }
                        }
                    }
                    let mut simd_accu = $splat;
                    while offset <= len_minus_stride {
                        simd_accu = simd_accu
                            | unsafe { *(src.offset(offset as isize) as *const $simd_ty) };
                        offset += SIMD_STRIDE_SIZE / unit_size;
                    }
                    if !$func(simd_accu) {
                        return false;
                    }
                }
            }
            for &unit in &buffer[offset..] {
                accu |= unit as usize;
            }
            accu < $bound
        }
    };
}

cfg_if!{
    if #[cfg(all(feature = "simd-accel", any(target_feature = "sse2", all(target_endian = "little", target_arch = "aarch64"), all(target_endian = "little", target_feature = "neon"))))] {
        use simd_funcs::*;
        use simd::u8x16;
        use simd::u16x8;

        const SIMD_ALIGNMENT: usize = 16;

        const SIMD_ALIGNMENT_MASK: usize = 15;

        by_unit_check_simd!(is_ascii_impl, u8, u8x16::splat(0), u8x16, 0x80, simd_is_ascii);
        by_unit_check_simd!(is_basic_latin_impl, u16, u16x8::splat(0), u16x8, 0x80, simd_is_basic_latin);
        by_unit_check_simd!(is_utf16_latin1_impl, u16, u16x8::splat(0), u16x8, 0x100, simd_is_latin1);

        #[inline(always)]
        fn utf16_valid_up_to_impl(buffer: &[u16]) -> usize {
            // This function is a mess, because it simultaneously tries to do
            // only aligned SIMD (perhaps misguidedly) and needs to deal with
            // the last code unit in a SIMD stride being part of a valid
            // surrogate pair.
            let unit_size = ::std::mem::size_of::<u16>();
            let src = buffer.as_ptr();
            let len = buffer.len();
            let mut offset = 0usize;
            'outer: loop {
                let until_alignment = ((SIMD_ALIGNMENT - ((unsafe { src.offset(offset as isize) } as usize) & SIMD_ALIGNMENT_MASK)) &
                                        SIMD_ALIGNMENT_MASK) / unit_size;
                if until_alignment == 0 {
                    if offset + SIMD_STRIDE_SIZE / unit_size > len {
                        break;
                    }
                } else {
                    let offset_plus_until_alignment = offset + until_alignment;
                    let offset_plus_until_alignment_plus_one = offset_plus_until_alignment + 1;
                    if offset_plus_until_alignment_plus_one + SIMD_STRIDE_SIZE / unit_size > len {
                        break;
                    }
                    let (up_to, last_valid_low) = utf16_valid_up_to_alu(&buffer[offset..offset_plus_until_alignment_plus_one]);
                    if up_to < until_alignment {
                        return offset + up_to;
                    }
                    if last_valid_low {
                        offset = offset_plus_until_alignment_plus_one;
                        continue;
                    }
                    offset = offset_plus_until_alignment;
                }
                let len_minus_stride = len - SIMD_STRIDE_SIZE / unit_size;
                'inner: loop {
                    let offset_plus_stride = offset + SIMD_STRIDE_SIZE / unit_size;
                    if contains_surrogates(unsafe { *(src.offset(offset as isize) as *const u16x8) }) {
                        if offset_plus_stride == len {
                            break 'outer;
                        }
                        let offset_plus_stride_plus_one = offset_plus_stride + 1;
                        let (up_to, last_valid_low) = utf16_valid_up_to_alu(&buffer[offset..offset_plus_stride_plus_one]);
                        if up_to < SIMD_STRIDE_SIZE / unit_size {
                            return offset + up_to;
                        }
                        if last_valid_low {
                            offset = offset_plus_stride_plus_one;
                            continue 'outer;
                        }
                    }
                    offset = offset_plus_stride;
                    if offset > len_minus_stride {
                        break 'outer;
                    }
                }
            }
            let (up_to, _) = utf16_valid_up_to_alu(&buffer[offset..]);
            offset + up_to
        }
    } else {
        by_unit_check_alu!(is_ascii_impl, u8, 0x80, ASCII_MASK);
        by_unit_check_alu!(is_basic_latin_impl, u16, 0x80, BASIC_LATIN_MASK);
        by_unit_check_alu!(is_utf16_latin1_impl, u16, 0x100, LATIN1_MASK);

        #[inline(always)]
        fn utf16_valid_up_to_impl(buffer: &[u16]) -> usize {
            let (up_to, _) = utf16_valid_up_to_alu(buffer);
            up_to
        }
    }
}

/// The second return value is true iff the last code unit of the slice was
/// reached and turned out to be a low surrogate that is part of a valid pair.
#[inline(always)]
fn utf16_valid_up_to_alu(buffer: &[u16]) -> (usize, bool) {
    let len = buffer.len();
    if len == 0 {
        return (0, false);
    }
    let mut offset = 0usize;
    loop {
        let unit = buffer[offset];
        let next = offset + 1;
        let unit_minus_surrogate_start = unit.wrapping_sub(0xD800);
        if unit_minus_surrogate_start > (0xDFFF - 0xD800) {
            // Not a surrogate
            offset = next;
            if offset == len {
                return (offset, false);
            }
            continue;
        }
        if unit_minus_surrogate_start <= (0xDBFF - 0xD800) {
            // high surrogate
            if next < len {
                let second = buffer[next];
                let second_minus_low_surrogate_start = second.wrapping_sub(0xDC00);
                if second_minus_low_surrogate_start <= (0xDFFF - 0xDC00) {
                    // The next code unit is a low surrogate. Advance position.
                    offset = next + 1;
                    if offset == len {
                        return (offset, true);
                    }
                    continue;
                }
                // The next code unit is not a low surrogate. Don't advance
                // position and treat the high surrogate as unpaired.
                // fall through
            }
            // Unpaired, fall through
        }
        // Unpaired surrogate
        return (offset, false);
    }
}

cfg_if!{
    if #[cfg(all(feature = "simd-accel", any(target_feature = "sse2", all(target_endian = "little", target_arch = "aarch64"), all(target_endian = "little", target_feature = "neon"))))] {
        #[inline(always)]
        fn is_str_latin1_impl(buffer: &str) -> Option<usize> {
            let mut offset = 0usize;
            let bytes = buffer.as_bytes();
            let len = bytes.len();
            if len >= SIMD_STRIDE_SIZE {
                let src = bytes.as_ptr();
                let mut until_alignment = (SIMD_ALIGNMENT - ((src as usize) & SIMD_ALIGNMENT_MASK)) &
                                           SIMD_ALIGNMENT_MASK;
                if until_alignment + SIMD_STRIDE_SIZE <= len {
                    while until_alignment != 0 {
                        if bytes[offset] > 0xC3 {
                            return Some(offset);
                        }
                        offset += 1;
                        until_alignment -= 1;
                    }
                    let len_minus_stride = len - SIMD_STRIDE_SIZE;
                    loop {
                        if !simd_is_str_latin1(unsafe { *(src.offset(offset as isize) as *const u8x16) }) {
                            // TODO: Ensure this compiles away when inlined into `is_str_latin1()`.
                            while bytes[offset] & 0xC0 == 0x80 {
                                offset += 1;
                            }
                            return Some(offset);
                        }
                        offset += SIMD_STRIDE_SIZE;
                        if offset > len_minus_stride {
                            break;
                        }
                    }
                }
            }
            for i in offset..len {
                if bytes[i] > 0xC3 {
                    return Some(i);
                }
            }
            None
        }
    } else {
        #[inline(always)]
        fn is_str_latin1_impl(buffer: &str) -> Option<usize> {
            let mut bytes = buffer.as_bytes();
            let mut total = 0;
            loop {
                if let Some((byte, offset)) = validate_ascii(bytes) {
                    total += offset;
                    if byte > 0xC3 {
                        return Some(total);
                    }
                    bytes = &bytes[offset + 2..];
                    total += 2;
                } else {
                    return None;
                }
            }
        }
    }
}

#[inline(always)]
fn is_utf8_latin1_impl(buffer: &[u8]) -> Option<usize> {
    let mut bytes = buffer;
    let mut total = 0;
    loop {
        if let Some((byte, offset)) = validate_ascii(bytes) {
            total += offset;
            if in_inclusive_range8(byte, 0xC2, 0xC3) {
                let next = offset + 1;
                if next == bytes.len() {
                    return Some(total);
                }
                if bytes[next] & 0xC0 != 0x80 {
                    return Some(total);
                }
                bytes = &bytes[offset + 2..];
                total += 2;
            } else {
                return Some(total);
            }
        } else {
            return None;
        }
    }
}

cfg_if!{
    if #[cfg(all(feature = "simd-accel", any(target_feature = "sse2", all(target_endian = "little", target_arch = "aarch64"), all(target_endian = "little", target_feature = "neon"))))] {
        #[inline(always)]
        fn is_utf16_bidi_impl(buffer: &[u16]) -> bool {
            let mut offset = 0usize;
            let len = buffer.len();
            if len >= SIMD_STRIDE_SIZE / 2 {
                let src = buffer.as_ptr();
                let mut until_alignment = ((SIMD_ALIGNMENT - ((src as usize) & SIMD_ALIGNMENT_MASK)) &
                                           SIMD_ALIGNMENT_MASK) / 2;
                if until_alignment + (SIMD_STRIDE_SIZE / 2) <= len {
                    while until_alignment != 0 {
                        if is_utf16_code_unit_bidi(buffer[offset]) {
                            return true;
                        }
                        offset += 1;
                        until_alignment -= 1;
                    }
                    let len_minus_stride = len - (SIMD_STRIDE_SIZE / 2);
                    loop {
                        if is_u16x8_bidi(unsafe { *(src.offset(offset as isize) as *const u16x8) }) {
                            return true;
                        }
                        offset += SIMD_STRIDE_SIZE / 2;
                        if offset > len_minus_stride {
                            break;
                        }
                    }
                }
            }
            for &u in &buffer[offset..] {
                if is_utf16_code_unit_bidi(u) {
                    return true;
                }
            }
            false
        }
    } else {
        #[inline(always)]
        fn is_utf16_bidi_impl(buffer: &[u16]) -> bool {
            for &u in buffer {
                if is_utf16_code_unit_bidi(u) {
                    return true;
                }
            }
            false
        }
    }
}

cfg_if!{
    if #[cfg(all(feature = "simd-accel", any(target_feature = "sse2", all(target_endian = "little", target_arch = "aarch64"), all(target_endian = "little", target_feature = "neon"))))] {
        #[inline(always)]
        fn check_utf16_for_latin1_and_bidi_impl(buffer: &[u16]) -> Latin1Bidi {
            let mut offset = 0usize;
            let len = buffer.len();
            if len >= SIMD_STRIDE_SIZE / 2 {
                let src = buffer.as_ptr();
                let mut until_alignment = ((SIMD_ALIGNMENT - ((src as usize) & SIMD_ALIGNMENT_MASK)) &
                                           SIMD_ALIGNMENT_MASK) / 2;
                if until_alignment + (SIMD_STRIDE_SIZE / 2) <= len {
                    while until_alignment != 0 {
                        if buffer[offset] > 0xFF {
                            // This transition isn't optimal, since the aligment is recomputing
                            // but not tweaking further today.
                            if is_utf16_bidi_impl(&buffer[offset..]) {
                                return Latin1Bidi::Bidi;
                            }
                            return Latin1Bidi::LeftToRight;
                        }
                        offset += 1;
                        until_alignment -= 1;
                    }
                    let len_minus_stride = len - (SIMD_STRIDE_SIZE / 2);
                    loop {
                        let mut s = unsafe { *(src.offset(offset as isize) as *const u16x8) };
                        if !simd_is_latin1(s) {
                            loop {
                                if is_u16x8_bidi(s) {
                                    return Latin1Bidi::Bidi;
                                }
                                offset += SIMD_STRIDE_SIZE / 2;
                                if offset > len_minus_stride {
                                    for &u in &buffer[offset..] {
                                        if is_utf16_code_unit_bidi(u) {
                                            return Latin1Bidi::Bidi;
                                        }
                                    }
                                    return Latin1Bidi::LeftToRight;
                                }
                                s = unsafe { *(src.offset(offset as isize) as *const u16x8) };
                            }
                        }
                        offset += SIMD_STRIDE_SIZE / 2;
                        if offset > len_minus_stride {
                            break;
                        }
                    }
                }
            }
            let mut iter = (&buffer[offset..]).iter();
            loop {
                if let Some(&u) = iter.next() {
                    if u > 0xFF {
                        let mut inner_u = u;
                        loop {
                            if is_utf16_code_unit_bidi(inner_u) {
                                return Latin1Bidi::Bidi;
                            }
                            if let Some(&code_unit) = iter.next() {
                                inner_u = code_unit;
                            } else {
                                return Latin1Bidi::LeftToRight;
                            }
                        }
                    }
                } else {
                    return Latin1Bidi::Latin1;
                }
            }
        }
    } else {
        #[inline(always)]
        fn check_utf16_for_latin1_and_bidi_impl(buffer: &[u16]) -> Latin1Bidi {
            let mut offset = 0usize;
            let len = buffer.len();
            if len >= ALU_ALIGNMENT / 2 {
                let src = buffer.as_ptr();
                let mut until_alignment = ((ALU_ALIGNMENT - ((src as usize) & ALU_ALIGNMENT_MASK)) &
                                           ALU_ALIGNMENT_MASK) / 2;
                if until_alignment + ALU_ALIGNMENT / 2 <= len {
                    while until_alignment != 0 {
                        if buffer[offset] > 0xFF {
                            if is_utf16_bidi_impl(&buffer[offset..]) {
                                return Latin1Bidi::Bidi;
                            }
                            return Latin1Bidi::LeftToRight;
                        }
                        offset += 1;
                        until_alignment -= 1;
                    }
                    let len_minus_stride = len - ALU_ALIGNMENT / 2;
                    loop {
                        if unsafe { *(src.offset(offset as isize) as *const usize) } & LATIN1_MASK != 0 {
                            if is_utf16_bidi_impl(&buffer[offset..]) {
                                return Latin1Bidi::Bidi;
                            }
                            return Latin1Bidi::LeftToRight;
                        }
                        offset += ALU_ALIGNMENT / 2;
                        if offset > len_minus_stride {
                            break;
                        }
                    }
                }
            }
            let mut iter = (&buffer[offset..]).iter();
            loop {
                if let Some(&u) = iter.next() {
                    if u > 0xFF {
                        let mut inner_u = u;
                        loop {
                            if is_utf16_code_unit_bidi(inner_u) {
                                return Latin1Bidi::Bidi;
                            }
                            if let Some(&code_unit) = iter.next() {
                                inner_u = code_unit;
                            } else {
                                return Latin1Bidi::LeftToRight;
                            }
                        }
                    }
                } else {
                    return Latin1Bidi::Latin1;
                }
            }
        }
    }
}

/// Checks whether the buffer is all-ASCII.
///
/// May read the entire buffer even if it isn't all-ASCII. (I.e. the function
/// is not guaranteed to fail fast.)
#[inline]
pub fn is_ascii(buffer: &[u8]) -> bool {
    is_ascii_impl(buffer)
}

/// Checks whether the buffer is all-Basic Latin (i.e. UTF-16 representing
/// only ASCII characters).
///
/// May read the entire buffer even if it isn't all-ASCII. (I.e. the function
/// is not guaranteed to fail fast.)
#[inline]
pub fn is_basic_latin(buffer: &[u16]) -> bool {
    is_basic_latin_impl(buffer)
}

/// Checks whether the buffer is valid UTF-8 representing only code points
/// less than or equal to U+00FF.
///
/// Fails fast. (I.e. returns before having read the whole buffer if UTF-8
/// invalidity or code points above U+00FF are discovered.
#[inline]
pub fn is_utf8_latin1(buffer: &[u8]) -> bool {
    is_utf8_latin1_impl(buffer).is_none()
}

/// Checks whether the buffer represents only code point less than or equal
/// to U+00FF.
///
/// Fails fast. (I.e. returns before having read the whole buffer if code
/// points above U+00FF are discovered.
#[inline]
pub fn is_str_latin1(buffer: &str) -> bool {
    is_str_latin1_impl(buffer).is_none()
}

/// Checks whether the buffer represents only code point less than or equal
/// to U+00FF.
///
/// May read the entire buffer even if it isn't all-Latin1. (I.e. the function
/// is not guaranteed to fail fast.)
#[inline]
pub fn is_utf16_latin1(buffer: &[u16]) -> bool {
    is_utf16_latin1_impl(buffer)
}

/// Checks whether a potentially-invalid UTF-8 buffer contains code points
/// that trigger right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Additionally, the four
/// RIGHT-TO-LEFT FOO controls in General Punctuation are checked for.
/// Control characters that are technically bidi controls but do not cause
/// right-to-left behavior without the presence of right-to-left characters
/// or right-to-left controls are not checked for.
///
/// Returns `true` if the input is invalid UTF-8 or the input contains an
/// RTL character. Returns `false` if the input is valid UTF-8 and contains
/// no RTL characters.
#[inline]
pub fn is_utf8_bidi(buffer: &[u8]) -> bool {
    // As of rustc 1.25.0-nightly (73ac5d6a8 2018-01-11), this is faster
    // than UTF-8 validation followed by `is_str_bidi()` for German,
    // Russian and Japanese. However, this is considerably slower for Thai.
    // Chances are that the compiler makes some branch predictions that are
    // unfortunate for Thai. Not spending the time to manually optimize
    // further at this time, since it's unclear if this variant even has
    // use cases. However, this is worth revisiting once Rust gets the
    // ability to annotate relative priorities of match arms.

    // U+058F: D6 8F
    // U+0590: D6 90
    // U+08FF: E0 A3 BF
    // U+0900: E0 A4 80
    //
    // U+200F: E2 80 8F
    // U+202B: E2 80 AB
    // U+202E: E2 80 AE
    // U+2067: E2 81 A7
    //
    // U+FB4F: EF AD 8F
    // U+FB50: EF AD 90
    // U+FDFF: EF B7 BF
    // U+FE00: EF B8 80
    //
    // U+FE6F: EF B9 AF
    // U+FE70: EF B9 B0
    // U+FEFF: EF BB BF
    // U+FF00: EF BC 80
    //
    // U+107FF: F0 90 9F BF
    // U+10800: F0 90 A0 80
    // U+10FFF: F0 90 BF BF
    // U+11000: F0 91 80 80
    //
    // U+1E7FF: F0 9E 9F BF
    // U+1E800: F0 9E A0 80
    // U+1EFFF: F0 9E BF BF
    // U+1F000: F0 9F 80 80
    let mut bytes = buffer;
    'outer: loop {
        if let Some((mut byte, mut read)) = validate_ascii(bytes) {
            // Check for the longest sequence to avoid checking twice for the
            // multi-byte sequences.
            if read + 4 <= bytes.len() {
                'inner: loop {
                    // At this point, `byte` is not included in `read`.
                    match byte {
                        0...0x7F => {
                            // ASCII: go back to SIMD.
                            read += 1;
                            bytes = &bytes[read..];
                            continue 'outer;
                        }
                        0xC2...0xD5 => {
                            // Two-byte
                            let second = bytes[read + 1];
                            if (UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL) != 0 {
                                return true;
                            }
                            read += 2;
                        }
                        0xD6 => {
                            // Two-byte
                            let second = bytes[read + 1];
                            if (UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL) != 0 {
                                return true;
                            }
                            // XXX consider folding the above and below checks
                            if second > 0x8F {
                                return true;
                            }
                            read += 2;
                        }
                        // two-byte starting with 0xD7 and above is bidi
                        0xE1 | 0xE3...0xEC | 0xEE => {
                            // Three-byte normal
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            read += 3;
                        }
                        0xE2 => {
                            // Three-byte normal, potentially bidi
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            if second == 0x80 {
                                if third == 0x8F || third == 0xAB || third == 0xAE {
                                    return true;
                                }
                            } else if second == 0x81 {
                                if third == 0xA7 {
                                    return true;
                                }
                            }
                            read += 3;
                        }
                        0xEF => {
                            // Three-byte normal, potentially bidi
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            if in_inclusive_range8(second, 0xAD, 0xB7) {
                                if second == 0xAD {
                                    if third > 0x8F {
                                        return true;
                                    }
                                } else {
                                    return true;
                                }
                            } else if in_inclusive_range8(second, 0xB9, 0xBB) {
                                if second == 0xB9 {
                                    if third > 0xAF {
                                        return true;
                                    }
                                } else {
                                    return true;
                                }
                            }
                            read += 3;
                        }
                        0xE0 => {
                            // Three-byte special lower bound, potentially bidi
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            if ((UTF8_TRAIL_INVALID[second as usize]
                                & UTF8_THREE_BYTE_SPECIAL_LOWER_BOUND_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            // XXX can this be folded into the above validity check
                            if second < 0xA4 {
                                return true;
                            }
                            read += 3;
                        }
                        0xED => {
                            // Three-byte special upper bound
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            if ((UTF8_TRAIL_INVALID[second as usize]
                                & UTF8_THREE_BYTE_SPECIAL_UPPER_BOUND_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            read += 3;
                        }
                        0xF1...0xF3 => {
                            // Four-byte normal
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            let fourth = bytes[read + 3];
                            if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[fourth as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            read += 4;
                        }
                        0xF0 => {
                            // Four-byte special lower bound, potentially bidi
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            let fourth = bytes[read + 3];
                            if ((UTF8_TRAIL_INVALID[second as usize]
                                & UTF8_FOUR_BYTE_SPECIAL_LOWER_BOUND_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[fourth as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            if unsafe { unlikely(second == 0x90 || second == 0x9E) } {
                                let third = bytes[read + 2];
                                if third >= 0xA0 {
                                    return true;
                                }
                            }
                            read += 4;
                        }
                        0xF4 => {
                            // Four-byte special upper bound
                            let second = bytes[read + 1];
                            let third = bytes[read + 2];
                            let fourth = bytes[read + 3];
                            if ((UTF8_TRAIL_INVALID[second as usize]
                                & UTF8_FOUR_BYTE_SPECIAL_UPPER_BOUND_TRAIL)
                                | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL)
                                | (UTF8_TRAIL_INVALID[fourth as usize] & UTF8_NORMAL_TRAIL))
                                != 0
                            {
                                return true;
                            }
                            read += 4;
                        }
                        _ => {
                            // Invalid lead or bidi-only lead
                            return true;
                        }
                    }
                    if read + 4 > bytes.len() {
                        if read == bytes.len() {
                            return false;
                        }
                        byte = bytes[read];
                        break 'inner;
                    }
                    byte = bytes[read];
                    continue 'inner;
                }
            }
            // We can't have a complete 4-byte sequence, but we could still have
            // a complete shorter sequence.

            // At this point, `byte` is not included in `read`.
            match byte {
                0...0x7F => {
                    // ASCII: go back to SIMD.
                    read += 1;
                    bytes = &bytes[read..];
                    continue 'outer;
                }
                0xC2...0xD5 => {
                    // Two-byte
                    let new_read = read + 2;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    if (UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL) != 0 {
                        return true;
                    }
                    read = new_read;
                    // We need to deal with the case where we came here with 3 bytes
                    // left, so we need to take a look at the last one.
                    bytes = &bytes[read..];
                    continue 'outer;
                }
                0xD6 => {
                    // Two-byte, potentially bidi
                    let new_read = read + 2;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    if (UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL) != 0 {
                        return true;
                    }
                    // XXX consider folding the above and below checks
                    if second > 0x8F {
                        return true;
                    }
                    read = new_read;
                    // We need to deal with the case where we came here with 3 bytes
                    // left, so we need to take a look at the last one.
                    bytes = &bytes[read..];
                    continue 'outer;
                }
                // two-byte starting with 0xD7 and above is bidi
                0xE1 | 0xE3...0xEC | 0xEE => {
                    // Three-byte normal
                    let new_read = read + 3;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    let third = bytes[read + 2];
                    if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                        | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                        != 0
                    {
                        return true;
                    }
                }
                0xE2 => {
                    // Three-byte normal, potentially bidi
                    let new_read = read + 3;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    let third = bytes[read + 2];
                    if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                        | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                        != 0
                    {
                        return true;
                    }
                    if second == 0x80 {
                        if third == 0x8F || third == 0xAB || third == 0xAE {
                            return true;
                        }
                    } else if second == 0x81 {
                        if third == 0xA7 {
                            return true;
                        }
                    }
                }
                0xEF => {
                    // Three-byte normal, potentially bidi
                    let new_read = read + 3;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    let third = bytes[read + 2];
                    if ((UTF8_TRAIL_INVALID[second as usize] & UTF8_NORMAL_TRAIL)
                        | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                        != 0
                    {
                        return true;
                    }
                    if in_inclusive_range8(second, 0xAD, 0xB7) {
                        if second == 0xAD {
                            if third > 0x8F {
                                return true;
                            }
                        } else {
                            return true;
                        }
                    } else if in_inclusive_range8(second, 0xB9, 0xBB) {
                        if second == 0xB9 {
                            if third > 0xAF {
                                return true;
                            }
                        } else {
                            return true;
                        }
                    }
                }
                0xE0 => {
                    // Three-byte special lower bound, potentially bidi
                    let new_read = read + 3;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    let third = bytes[read + 2];
                    if ((UTF8_TRAIL_INVALID[second as usize]
                        & UTF8_THREE_BYTE_SPECIAL_LOWER_BOUND_TRAIL)
                        | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                        != 0
                    {
                        return true;
                    }
                    // XXX can this be folded into the above validity check
                    if second < 0xA4 {
                        return true;
                    }
                }
                0xED => {
                    // Three-byte special upper bound
                    let new_read = read + 3;
                    if new_read > bytes.len() {
                        return true;
                    }
                    let second = bytes[read + 1];
                    let third = bytes[read + 2];
                    if ((UTF8_TRAIL_INVALID[second as usize]
                        & UTF8_THREE_BYTE_SPECIAL_UPPER_BOUND_TRAIL)
                        | (UTF8_TRAIL_INVALID[third as usize] & UTF8_NORMAL_TRAIL))
                        != 0
                    {
                        return true;
                    }
                }
                _ => {
                    // Invalid lead, 4-byte lead or 2-byte bidi-only lead
                    return true;
                }
            }
            return false;
        } else {
            return false;
        }
    }
}

/// Checks whether a valid UTF-8 buffer contains code points that trigger
/// right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Additionally, the four
/// RIGHT-TO-LEFT FOO controls in General Punctuation are checked for.
/// Control characters that are technically bidi controls but do not cause
/// right-to-left behavior without the presence of right-to-left characters
/// or right-to-left controls are not checked for.
#[inline]
pub fn is_str_bidi(buffer: &str) -> bool {
    // U+058F: D6 8F
    // U+0590: D6 90
    // U+08FF: E0 A3 BF
    // U+0900: E0 A4 80
    //
    // U+200F: E2 80 8F
    // U+202B: E2 80 AB
    // U+202E: E2 80 AE
    // U+2067: E2 81 A7
    //
    // U+FB4F: EF AD 8F
    // U+FB50: EF AD 90
    // U+FDFF: EF B7 BF
    // U+FE00: EF B8 80
    //
    // U+FE6F: EF B9 AF
    // U+FE70: EF B9 B0
    // U+FEFF: EF BB BF
    // U+FF00: EF BC 80
    //
    // U+107FF: F0 90 9F BF
    // U+10800: F0 90 A0 80
    // U+10FFF: F0 90 BF BF
    // U+11000: F0 91 80 80
    //
    // U+1E7FF: F0 9E 9F BF
    // U+1E800: F0 9E A0 80
    // U+1EFFF: F0 9E BF BF
    // U+1F000: F0 9F 80 80
    let mut bytes = buffer.as_bytes();
    'outer: loop {
        // TODO: Instead of just validating ASCII using SIMD, use SIMD
        // to check for non-ASCII lead bytes, too, to quickly conclude
        // that the vector consist entirely of CJK and below-Hebrew
        // code points.
        // Unfortunately, scripts above Arabic but below CJK share
        // lead bytes with RTL.
        if let Some((mut byte, mut read)) = validate_ascii(bytes) {
            'inner: loop {
                // At this point, `byte` is not included in `read`.
                if byte < 0xE0 {
                    if byte >= 0x80 {
                        // Two-byte
                        // Adding `unlikely` here improved throughput on
                        // Russian plain text by 33%!
                        if unsafe { unlikely(byte >= 0xD6) } {
                            if byte == 0xD6 {
                                let second = bytes[read + 1];
                                if second > 0x8F {
                                    return true;
                                }
                            } else {
                                return true;
                            }
                        }
                        read += 2;
                    } else {
                        // ASCII: write and go back to SIMD.
                        read += 1;
                        // Intuitively, we should go back to the outer loop only
                        // if byte is 0x30 or above, so as to avoid trashing on
                        // ASCII space, comma and period in non-Latin context.
                        // However, the extra branch seems to cost more than it's
                        // worth.
                        bytes = &bytes[read..];
                        continue 'outer;
                    }
                } else if byte < 0xF0 {
                    // Three-byte
                    if unsafe { unlikely(!in_inclusive_range8(byte, 0xE3, 0xEE) && byte != 0xE1) } {
                        let second = bytes[read + 1];
                        if byte == 0xE0 {
                            if second < 0xA4 {
                                return true;
                            }
                        } else if byte == 0xE2 {
                            let third = bytes[read + 2];
                            if second == 0x80 {
                                if third == 0x8F || third == 0xAB || third == 0xAE {
                                    return true;
                                }
                            } else if second == 0x81 {
                                if third == 0xA7 {
                                    return true;
                                }
                            }
                        } else {
                            debug_assert_eq!(byte, 0xEF);
                            if in_inclusive_range8(second, 0xAD, 0xB7) {
                                if second == 0xAD {
                                    let third = bytes[read + 2];
                                    if third > 0x8F {
                                        return true;
                                    }
                                } else {
                                    return true;
                                }
                            } else if in_inclusive_range8(second, 0xB9, 0xBB) {
                                if second == 0xB9 {
                                    let third = bytes[read + 2];
                                    if third > 0xAF {
                                        return true;
                                    }
                                } else {
                                    return true;
                                }
                            }
                        }
                    }
                    read += 3;
                } else {
                    // Four-byte
                    let second = bytes[read + 1];
                    if unsafe { unlikely(byte == 0xF0 && (second == 0x90 || second == 0x9E)) } {
                        let third = bytes[read + 2];
                        if third >= 0xA0 {
                            return true;
                        }
                    }
                    read += 4;
                }
                // The comparison is always < or == and never >, but including
                // > here to let the compiler assume that < is true if this
                // comparison is false.
                if read >= bytes.len() {
                    return false;
                }
                byte = bytes[read];
                continue 'inner;
            }
        } else {
            return false;
        }
    }
}

/// Checks whether a UTF-16 buffer contains code points that trigger
/// right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Additionally, the four
/// RIGHT-TO-LEFT FOO controls in General Punctuation are checked for.
/// Control characters that are technically bidi controls but do not cause
/// right-to-left behavior without the presence of right-to-left characters
/// or right-to-left controls are not checked for.
///
/// Returns `true` if the input contains an RTL character or an unpaired
/// high surrogate that could be the high half of an RTL character.
/// Returns `false` if teh input contains neither RTL characters nor
/// unpaired high surrogates that could be higher halves of RTL characters.
#[inline]
pub fn is_utf16_bidi(buffer: &[u16]) -> bool {
    is_utf16_bidi_impl(buffer)
}

/// Checks whether a code point triggers right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Additionally, the four
/// RIGHT-TO-LEFT FOO controls in General Punctuation are checked for.
/// Control characters that are technically bidi controls but do not cause
/// right-to-left behavior without the presence of right-to-left characters
/// or right-to-left controls are not checked for.
#[inline(always)]
pub fn is_char_bidi(c: char) -> bool {
    // Controls:
    // Every control with RIGHT-TO-LEFT in its name in
    // https://www.unicode.org/charts/PDF/U2000.pdf
    // U+200F RLM
    // U+202B RLE
    // U+202E RLO
    // U+2067 RLI
    //
    // BMP RTL:
    // https://www.unicode.org/roadmaps/bmp/
    // U+0590...U+08FF
    // U+FB50...U+FDFF Arabic Presentation Forms A
    // U+FE70...U+FEFF Arabic Presentation Forms B
    //
    // Supplementary RTL:
    // https://www.unicode.org/roadmaps/smp/
    // U+10800...U+10FFF (Lead surrogate U+D802 or U+D803)
    // U+1E800...U+1EFFF (Lead surrogate U+D83A or U+D83B)
    let code_point = c as u32;
    if code_point < 0x0590 {
        // Below Hebrew
        return false;
    }
    if in_range32(code_point, 0x0900, 0xFB50) {
        // Above Arabic Extended-A and below Arabic Presentation Forms
        if in_inclusive_range32(code_point, 0x200F, 0x2067) {
            // In the range that contains the RTL controls
            return code_point == 0x200F
                || code_point == 0x202B
                || code_point == 0x202E
                || code_point == 0x2067;
        }
        return false;
    }
    if code_point > 0x1EFFF {
        // Above second astral RTL. (Emoji is here.)
        return false;
    }
    if in_range32(code_point, 0x11000, 0x1E800) {
        // Between astral RTL blocks
        return false;
    }
    if in_range32(code_point, 0xFF00, 0x10800) {
        // Above Arabic Presentations Forms B and below first
        // astral RTL
        return false;
    }
    if in_range32(code_point, 0xFE00, 0xFE70) {
        // Between Arabic Presentations Forms
        return false;
    }
    true
}

/// Checks whether a UTF-16 code unit triggers right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Additionally, the four
/// RIGHT-TO-LEFT FOO controls in General Punctuation are checked for.
/// Control characters that are technically bidi controls but do not cause
/// right-to-left behavior without the presence of right-to-left characters
/// or right-to-left controls are not checked for.
///
/// Since supplementary-plane right-to-left blocks are identifiable from the
/// high surrogate without examining the low surrogate, this function returns
/// `true` for such high surrogates making the function suitable for handling
/// supplementary-plane text without decoding surrogate pairs to scalar
/// values. Obviously, such high surrogates are then reported as right-to-left
/// even if actually unpaired.
#[inline(always)]
pub fn is_utf16_code_unit_bidi(u: u16) -> bool {
    if u < 0x0590 {
        // Below Hebrew
        return false;
    }
    if in_range16(u, 0x0900, 0xD802) {
        // Above Arabic Extended-A and below first RTL surrogate
        if in_inclusive_range16(u, 0x200F, 0x2067) {
            // In the range that contains the RTL controls
            return u == 0x200F || u == 0x202B || u == 0x202E || u == 0x2067;
        }
        return false;
    }
    if in_range16(u, 0xD83C, 0xFB50) {
        // Between astral RTL high surrogates and Arabic Presentation Forms
        // (Emoji is here)
        return false;
    }
    if in_range16(u, 0xD804, 0xD83A) {
        // Between RTL high surragates
        return false;
    }
    if u > 0xFEFF {
        // Above Arabic Presentation Forms
        return false;
    }
    if in_range16(u, 0xFE00, 0xFE70) {
        // Between Arabic Presentations Forms
        return false;
    }
    true
}

/// Checks whether a potentially invalid UTF-8 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_utf8_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_utf8_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
#[inline]
pub fn check_utf8_for_latin1_and_bidi(buffer: &[u8]) -> Latin1Bidi {
    if let Some(offset) = is_utf8_latin1_impl(buffer) {
        if is_utf8_bidi(&buffer[offset..]) {
            Latin1Bidi::Bidi
        } else {
            Latin1Bidi::LeftToRight
        }
    } else {
        Latin1Bidi::Latin1
    }
}

/// Checks whether a valid UTF-8 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_str_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_str_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
#[inline]
pub fn check_str_for_latin1_and_bidi(buffer: &str) -> Latin1Bidi {
    // The transition from the latin1 check to the bidi check isn't
    // optimal but not tweaking it to perfection today.
    if let Some(offset) = is_str_latin1_impl(buffer) {
        if is_str_bidi(&buffer[offset..]) {
            Latin1Bidi::Bidi
        } else {
            Latin1Bidi::LeftToRight
        }
    } else {
        Latin1Bidi::Latin1
    }
}

/// Checks whether a potentially invalid UTF-16 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_utf16_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_utf16_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
#[inline]
pub fn check_utf16_for_latin1_and_bidi(buffer: &[u16]) -> Latin1Bidi {
    check_utf16_for_latin1_and_bidi_impl(buffer)
}

/// Converts potentially-invalid UTF-8 to valid UTF-16 with errors replaced
/// with the REPLACEMENT CHARACTER.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer _plus one_.
///
/// Returns the number of `u16`s written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_utf8_to_utf16(src: &[u8], dst: &mut [u16]) -> usize {
    // TODO: Can the + 1 be eliminated?
    assert!(dst.len() >= src.len() + 1);
    let mut decoder = Utf8Decoder::new_inner();
    let mut total_read = 0usize;
    let mut total_written = 0usize;
    loop {
        let (result, read, written) =
            decoder.decode_to_utf16_raw(&src[total_read..], &mut dst[total_written..], true);
        total_read += read;
        total_written += written;
        match result {
            DecoderResult::InputEmpty => {
                return total_written;
            }
            DecoderResult::OutputFull => {
                unreachable!("The assert at the top of the function should have caught this.");
            }
            DecoderResult::Malformed(_, _) => {
                // There should always be space for the U+FFFD, because
                // otherwise we'd have gotten OutputFull already.
                dst[total_written] = 0xFFFD;
                total_written += 1;
            }
        }
    }
}

/// Converts valid UTF-8 to valid UTF-16.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of `u16`s written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_str_to_utf16(src: &str, dst: &mut [u16]) -> usize {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    let bytes = src.as_bytes();
    let mut read = 0;
    let mut written = 0;
    'outer: loop {
        let mut byte = {
            let src_remaining = &bytes[read..];
            let dst_remaining = &mut dst[written..];
            let length = src_remaining.len();
            match unsafe {
                ascii_to_basic_latin(src_remaining.as_ptr(), dst_remaining.as_mut_ptr(), length)
            } {
                None => {
                    written += length;
                    return written;
                }
                Some((non_ascii, consumed)) => {
                    read += consumed;
                    written += consumed;
                    non_ascii
                }
            }
        };
        'inner: loop {
            // At this point, `byte` is not included in `read`.
            if byte < 0xE0 {
                if byte >= 0x80 {
                    // Two-byte
                    let second = bytes[read + 1];
                    let point = (((byte as u32) & 0x1Fu32) << 6) | (second as u32 & 0x3Fu32);
                    dst[written] = point as u16;
                    read += 2;
                    written += 1;
                } else {
                    // ASCII: write and go back to SIMD.
                    dst[written] = byte as u16;
                    read += 1;
                    written += 1;
                    // Intuitively, we should go back to the outer loop only
                    // if byte is 0x30 or above, so as to avoid trashing on
                    // ASCII space, comma and period in non-Latin context.
                    // However, the extra branch seems to cost more than it's
                    // worth.
                    continue 'outer;
                }
            } else if byte < 0xF0 {
                // Three-byte
                let second = bytes[read + 1];
                let third = bytes[read + 2];
                let point = (((byte as u32) & 0xFu32) << 12)
                    | ((second as u32 & 0x3Fu32) << 6)
                    | (third as u32 & 0x3Fu32);
                dst[written] = point as u16;
                read += 3;
                written += 1;
            } else {
                // Four-byte
                let second = bytes[read + 1];
                let third = bytes[read + 2];
                let fourth = bytes[read + 3];
                let point = (((byte as u32) & 0x7u32) << 18)
                    | ((second as u32 & 0x3Fu32) << 12)
                    | ((third as u32 & 0x3Fu32) << 6)
                    | (fourth as u32 & 0x3Fu32);
                dst[written] = (0xD7C0 + (point >> 10)) as u16;
                dst[written + 1] = (0xDC00 + (point & 0x3FF)) as u16;
                read += 4;
                written += 2;
            }
            // The comparison is always < or == and never >, but including
            // > here to let the compiler assume that < is true if this
            // comparison is false.
            if read >= src.len() {
                return written;
            }
            byte = bytes[read];
            continue 'inner;
        }
    }
}

/// Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
/// with the REPLACEMENT CHARACTER.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer times three _plus one_.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
///
/// # Safety
///
/// Note that this function may write garbage beyond the number of bytes
/// indicated by the return value, so using a `&mut str` interpreted as
/// `&mut [u8]` as the destination is not safe. If you want to convert into
/// a `&mut str`, use `convert_utf16_to_str()` instead of this function.
#[inline]
pub fn convert_utf16_to_utf8(src: &[u16], dst: &mut [u8]) -> usize {
    assert!(dst.len() >= src.len() * 3 + 1);
    let mut encoder = Utf8Encoder;
    let (result, _, written) = encoder.encode_from_utf16_raw(src, dst, true);
    debug_assert!(result == EncoderResult::InputEmpty);
    written
}

/// Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
/// with the REPLACEMENT CHARACTER such that the validity of the output is
/// signaled using the Rust type system.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer times three _plus one_.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_utf16_to_str(src: &[u16], dst: &mut str) -> usize {
    let bytes: &mut [u8] = unsafe { ::std::mem::transmute(dst) };
    let written = convert_utf16_to_utf8(src, bytes);
    let len = bytes.len();
    let mut trail = written;
    let max = ::std::cmp::min(len, trail + MAX_STRIDE_SIZE);
    while trail < max {
        bytes[trail] = 0;
        trail += 1;
    }
    while trail < len && ((bytes[trail] & 0xC0) == 0x80) {
        bytes[trail] = 0;
        trail += 1;
    }
    written
}

/// Converts bytes whose unsigned value is interpreted as Unicode code point
/// (i.e. U+0000 to U+00FF, inclusive) to UTF-16.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// The number of `u16`s written equals the length of the source buffer.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_latin1_to_utf16(src: &[u8], dst: &mut [u16]) {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    // TODO: On aarch64, the safe version autovectorizes to the same unpacking
    // instructions and this code, but, yet, the autovectorized version is
    // faster.
    unsafe {
        unpack_latin1(src.as_ptr(), dst.as_mut_ptr(), src.len());
    }
}

/// Converts bytes whose unsigned value is interpreted as Unicode code point
/// (i.e. U+0000 to U+00FF, inclusive) to UTF-8.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer times two.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
///
/// # Safety
///
/// Note that this function may write garbage beyond the number of bytes
/// indicated by the return value, so using a `&mut str` interpreted as
/// `&mut [u8]` as the destination is not safe. If you want to convert into
/// a `&mut str`, use `convert_utf16_to_str()` instead of this function.
#[inline]
pub fn convert_latin1_to_utf8(src: &[u8], dst: &mut [u8]) -> usize {
    assert!(
        dst.len() >= src.len() * 2,
        "Destination must not be shorter than the source times two."
    );
    let src_len = src.len();
    let src_ptr = src.as_ptr();
    let dst_ptr = dst.as_mut_ptr();
    let mut total_read = 0usize;
    let mut total_written = 0usize;
    loop {
        // src can't advance more than dst
        let src_left = src_len - total_read;
        if let Some((non_ascii, consumed)) = unsafe {
            ascii_to_ascii(
                src_ptr.offset(total_read as isize),
                dst_ptr.offset(total_written as isize),
                src_left,
            )
        } {
            total_read += consumed + 1;
            total_written += consumed;

            let code_point = non_ascii as u32;
            dst[total_written] = ((code_point >> 6) | 0xC0u32) as u8;
            total_written += 1;
            dst[total_written] = ((code_point as u32 & 0x3Fu32) | 0x80u32) as u8;
            total_written += 1;
            continue;
        }
        return total_written + src_left;
    }
}

/// Converts bytes whose unsigned value is interpreted as Unicode code point
/// (i.e. U+0000 to U+00FF, inclusive) to UTF-8 such that the validity of the
/// output is signaled using the Rust type system.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer times two.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_latin1_to_str(src: &[u8], dst: &mut str) -> usize {
    let bytes: &mut [u8] = unsafe { ::std::mem::transmute(dst) };
    let written = convert_latin1_to_utf8(src, bytes);
    let len = bytes.len();
    let mut trail = written;
    let max = ::std::cmp::min(len, trail + MAX_STRIDE_SIZE);
    while trail < max {
        bytes[trail] = 0;
        trail += 1;
    }
    while trail < len && ((bytes[trail] & 0xC0) == 0x80) {
        bytes[trail] = 0;
        trail += 1;
    }
    written
}

/// If the input is valid UTF-8 representing only Unicode code points from
/// U+0000 to U+00FF, inclusive, converts the input into output that
/// represents the value of each code point as the unsigned byte value of
/// each output byte.
///
/// If the input does not fulfill the condition stated above, this function
/// does something that is memory-safe without any promises about any
/// properties of the output. In particular, callers shouldn't assume the
/// output to be the same across crate versions or CPU architectures and
/// should not assume that non-ASCII input can't map to ASCII output.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_utf8_to_latin1_lossy(src: &[u8], dst: &mut [u8]) -> usize {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    let src_len = src.len();
    let src_ptr = src.as_ptr();
    let dst_ptr = dst.as_mut_ptr();
    let mut total_read = 0usize;
    let mut total_written = 0usize;
    loop {
        // dst can't advance more than src
        let src_left = src_len - total_read;
        if let Some((non_ascii, consumed)) = unsafe {
            ascii_to_ascii(
                src_ptr.offset(total_read as isize),
                dst_ptr.offset(total_written as isize),
                src_left,
            )
        } {
            total_read += consumed + 1;
            total_written += consumed;

            if total_read == src_len {
                return total_written;
            }

            let trail = src[total_read];
            total_read += 1;

            dst[total_written] =
                (((non_ascii as u32 & 0x1Fu32) << 6) | (trail as u32 & 0x3Fu32)) as u8;
            total_written += 1;
            continue;
        }
        return total_written + src_left;
    }
}

/// If the input is valid UTF-16 representing only Unicode code points from
/// U+0000 to U+00FF, inclusive, converts the input into output that
/// represents the value of each code point as the unsigned byte value of
/// each output byte.
///
/// If the input does not fulfill the condition stated above, this function
/// does something that is memory-safe without any promises about any
/// properties of the output. In particular, callers shouldn't assume the
/// output to be the same across crate versions or CPU architectures and
/// should not assume that non-Basic Latin input can't map to ASCII output.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// The number of bytes written equals the length of the source buffer.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn convert_utf16_to_latin1_lossy(src: &[u16], dst: &mut [u8]) {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    unsafe {
        pack_latin1(src.as_ptr(), dst.as_mut_ptr(), src.len());
    }
}

/// Returns the index of the first unpaired surrogate or, if the input is
/// valid UTF-16 in its entirety, the length of the input.
#[inline]
pub fn utf16_valid_up_to(buffer: &[u16]) -> usize {
    utf16_valid_up_to_impl(buffer)
}

/// Replaces unpaired surrogates in the input with the REPLACEMENT CHARACTER.
#[inline]
pub fn ensure_utf16_validity(buffer: &mut [u16]) {
    let mut offset = 0;
    loop {
        offset += utf16_valid_up_to(&buffer[offset..]);
        if offset == buffer.len() {
            return;
        }
        buffer[offset] = 0xFFFD;
        offset += 1;
    }
}

/// Copies ASCII from source to destination up to the first non-ASCII byte
/// (or the end of the input if it is ASCII in its entirety).
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn copy_ascii_to_ascii(src: &[u8], dst: &mut [u8]) -> usize {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    if let Some((_, consumed)) =
        unsafe { ascii_to_ascii(src.as_ptr(), dst.as_mut_ptr(), src.len()) }
    {
        consumed
    } else {
        src.len()
    }
}

/// Copies ASCII from source to destination zero-extending it to UTF-16 up to
/// the first non-ASCII byte (or the end of the input if it is ASCII in its
/// entirety).
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of `u16`s written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn copy_ascii_to_basic_latin(src: &[u8], dst: &mut [u16]) -> usize {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    if let Some((_, consumed)) =
        unsafe { ascii_to_basic_latin(src.as_ptr(), dst.as_mut_ptr(), src.len()) }
    {
        consumed
    } else {
        src.len()
    }
}

/// Copies Basic Latin from source to destination narrowing it to ASCII up to
/// the first non-Basic Latin code unit (or the end of the input if it is
/// Basic Latin in its entirety).
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
#[inline]
pub fn copy_basic_latin_to_ascii(src: &[u16], dst: &mut [u8]) -> usize {
    assert!(
        dst.len() >= src.len(),
        "Destination must not be shorter than the source."
    );
    if let Some((_, consumed)) =
        unsafe { basic_latin_to_ascii(src.as_ptr(), dst.as_mut_ptr(), src.len()) }
    {
        consumed
    } else {
        src.len()
    }
}

// Any copyright to the test code below this comment is dedicated to the
// Public Domain. http://creativecommons.org/publicdomain/zero/1.0/

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_is_ascii_success() {
        let mut src: Vec<u8> = Vec::with_capacity(128);
        src.resize(128, 0);
        for i in 0..src.len() {
            src[i] = i as u8;
        }
        for i in 0..src.len() {
            assert!(is_ascii(&src[i..]));
        }
    }

    #[test]
    fn test_is_ascii_fail() {
        let mut src: Vec<u8> = Vec::with_capacity(128);
        src.resize(128, 0);
        for i in 0..src.len() {
            src[i] = i as u8;
        }
        for i in 0..src.len() {
            let tail = &mut src[i..];
            for j in 0..tail.len() {
                tail[j] = 0xA0;
                assert!(!is_ascii(tail));
            }
        }
    }

    #[test]
    fn test_is_basic_latin_success() {
        let mut src: Vec<u16> = Vec::with_capacity(128);
        src.resize(128, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            assert!(is_basic_latin(&src[i..]));
        }
    }

    #[test]
    fn test_is_basic_latin_fail() {
        let mut src: Vec<u16> = Vec::with_capacity(128);
        src.resize(128, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let tail = &mut src[i..];
            for j in 0..tail.len() {
                tail[j] = 0xA0;
                assert!(!is_basic_latin(tail));
            }
        }
    }

    #[test]
    fn test_is_utf16_latin1_success() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            assert!(is_utf16_latin1(&src[i..]));
            assert_eq!(
                check_utf16_for_latin1_and_bidi(&src[i..]),
                Latin1Bidi::Latin1
            );
        }
    }

    #[test]
    fn test_is_utf16_latin1_fail() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let tail = &mut src[i..];
            for j in 0..tail.len() {
                tail[j] = 0x100 + j as u16;
                assert!(!is_utf16_latin1(tail));
                assert_ne!(check_utf16_for_latin1_and_bidi(tail), Latin1Bidi::Latin1);
            }
        }
    }

    #[test]
    fn test_is_str_latin1_success() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let s = String::from_utf16(&src[i..]).unwrap();
            assert!(is_str_latin1(&s[..]));
            assert_eq!(check_str_for_latin1_and_bidi(&s[..]), Latin1Bidi::Latin1);
        }
    }

    #[test]
    fn test_is_str_latin1_fail() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let tail = &mut src[i..];
            for j in 0..tail.len() {
                tail[j] = 0x100 + j as u16;
                let s = String::from_utf16(tail).unwrap();
                assert!(!is_str_latin1(&s[..]));
                assert_ne!(check_str_for_latin1_and_bidi(&s[..]), Latin1Bidi::Latin1);
            }
        }
    }

    #[test]
    fn test_is_utf8_latin1_success() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let s = String::from_utf16(&src[i..]).unwrap();
            assert!(is_utf8_latin1(s.as_bytes()));
            assert_eq!(
                check_utf8_for_latin1_and_bidi(s.as_bytes()),
                Latin1Bidi::Latin1
            );
        }
    }

    #[test]
    fn test_is_utf8_latin1_fail() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        for i in 0..src.len() {
            src[i] = i as u16;
        }
        for i in 0..src.len() {
            let tail = &mut src[i..];
            for j in 0..tail.len() {
                tail[j] = 0x100 + j as u16;
                let s = String::from_utf16(tail).unwrap();
                assert!(!is_utf8_latin1(s.as_bytes()));
                assert_ne!(
                    check_utf8_for_latin1_and_bidi(s.as_bytes()),
                    Latin1Bidi::Latin1
                );
            }
        }
    }

    #[test]
    fn test_is_utf8_latin1_invalid() {
        assert!(!is_utf8_latin1(b"\xC3"));
        assert!(!is_utf8_latin1(b"a\xC3"));
        assert!(!is_utf8_latin1(b"\xFF"));
        assert!(!is_utf8_latin1(b"a\xFF"));
        assert!(!is_utf8_latin1(b"\xC3\xFF"));
        assert!(!is_utf8_latin1(b"a\xC3\xFF"));
    }

    #[test]
    fn test_convert_utf8_to_utf16() {
        let src = "abcdefghijklmnopqrstu\u{1F4A9}v\u{2603}w\u{00B6}xyzz";
        let mut dst: Vec<u16> = Vec::with_capacity(src.len() + 1);
        dst.resize(src.len() + 1, 0);
        let len = convert_utf8_to_utf16(src.as_bytes(), &mut dst[..]);
        dst.truncate(len);
        let reference: Vec<u16> = src.encode_utf16().collect();
        assert_eq!(dst, reference);
    }

    #[test]
    fn test_convert_str_to_utf16() {
        let src = "abcdefghijklmnopqrstu\u{1F4A9}v\u{2603}w\u{00B6}xyzz";
        let mut dst: Vec<u16> = Vec::with_capacity(src.len());
        dst.resize(src.len(), 0);
        let len = convert_str_to_utf16(src, &mut dst[..]);
        dst.truncate(len);
        let reference: Vec<u16> = src.encode_utf16().collect();
        assert_eq!(dst, reference);
    }

    #[test]
    fn test_convert_utf16_to_utf8() {
        let reference = "abcdefghijklmnopqrstu\u{1F4A9}v\u{2603}w\u{00B6}xyzz";
        let src: Vec<u16> = reference.encode_utf16().collect();
        let mut dst: Vec<u8> = Vec::with_capacity(src.len() * 3 + 1);
        dst.resize(src.len() * 3 + 1, 0);
        let len = convert_utf16_to_utf8(&src[..], &mut dst[..]);
        dst.truncate(len);
        assert_eq!(dst, reference.as_bytes());
    }

    #[test]
    fn test_convert_latin1_to_utf16() {
        let mut src: Vec<u8> = Vec::with_capacity(256);
        src.resize(256, 0);
        let mut reference: Vec<u16> = Vec::with_capacity(256);
        reference.resize(256, 0);
        for i in 0..256 {
            src[i] = i as u8;
            reference[i] = i as u16;
        }
        let mut dst: Vec<u16> = Vec::with_capacity(src.len());
        dst.resize(src.len(), 0);
        convert_latin1_to_utf16(&src[..], &mut dst[..]);
        assert_eq!(dst, reference);
    }

    #[test]
    fn test_convert_latin1_to_utf8() {
        let mut src: Vec<u8> = Vec::with_capacity(256);
        src.resize(256, 0);
        let mut reference: Vec<u16> = Vec::with_capacity(256);
        reference.resize(256, 0);
        for i in 0..256 {
            src[i] = i as u8;
            reference[i] = i as u16;
        }
        let s = String::from_utf16(&reference[..]).unwrap();
        let mut dst: Vec<u8> = Vec::with_capacity(src.len() * 2);
        dst.resize(src.len() * 2, 0);
        let len = convert_latin1_to_utf8(&src[..], &mut dst[..]);
        dst.truncate(len);
        assert_eq!(&dst[..], s.as_bytes());
    }

    #[test]
    fn test_convert_utf8_to_latin1_lossy() {
        let mut reference: Vec<u8> = Vec::with_capacity(256);
        reference.resize(256, 0);
        let mut src16: Vec<u16> = Vec::with_capacity(256);
        src16.resize(256, 0);
        for i in 0..256 {
            src16[i] = i as u16;
            reference[i] = i as u8;
        }
        let src = String::from_utf16(&src16[..]).unwrap();
        let mut dst: Vec<u8> = Vec::with_capacity(src.len());
        dst.resize(src.len(), 0);
        let len = convert_utf8_to_latin1_lossy(src.as_bytes(), &mut dst[..]);
        dst.truncate(len);
        assert_eq!(dst, reference);
    }

    #[test]
    fn test_convert_utf16_to_latin1_lossy() {
        let mut src: Vec<u16> = Vec::with_capacity(256);
        src.resize(256, 0);
        let mut reference: Vec<u8> = Vec::with_capacity(256);
        reference.resize(256, 0);
        for i in 0..256 {
            src[i] = i as u16;
            reference[i] = i as u8;
        }
        let mut dst: Vec<u8> = Vec::with_capacity(src.len());
        dst.resize(src.len(), 0);
        convert_utf16_to_latin1_lossy(&src[..], &mut dst[..]);
        assert_eq!(dst, reference);
    }

    #[test]
    fn test_utf16_valid_up_to() {
        let valid = vec![
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0x2603u16,
            0xD83Du16, 0xDCA9u16, 0x00B6u16,
        ];
        assert_eq!(utf16_valid_up_to(&valid[..]), 16);;
        let lone_high = vec![
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0x2603u16, 0xD83Du16, 0x00B6u16,
        ];
        assert_eq!(utf16_valid_up_to(&lone_high[..]), 14);;
        let lone_low = vec![
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0x2603u16, 0xDCA9u16, 0x00B6u16,
        ];
        assert_eq!(utf16_valid_up_to(&lone_low[..]), 14);;
        let lone_high_at_end = vec![
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0x2603u16, 0x00B6u16, 0xD83Du16,
        ];
        assert_eq!(utf16_valid_up_to(&lone_high_at_end[..]), 15);;
    }

    #[test]
    fn test_ensure_utf16_validity() {
        let mut src = vec![
            0u16, 0xD83Du16, 0u16, 0u16, 0u16, 0xD83Du16, 0xDCA9u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0u16, 0xDCA9u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
        ];
        let reference = vec![
            0u16, 0xFFFDu16, 0u16, 0u16, 0u16, 0xD83Du16, 0xDCA9u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0u16, 0xFFFDu16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
            0u16, 0u16, 0u16, 0u16, 0u16, 0u16,
        ];
        ensure_utf16_validity(&mut src[..]);
        assert_eq!(src, reference);
    }

    #[test]
    fn test_is_char_bidi() {
        assert!(!is_char_bidi('a'));
        assert!(!is_char_bidi('\u{03B1}'));
        assert!(!is_char_bidi('\u{3041}'));
        assert!(!is_char_bidi('\u{1F4A9}'));
        assert!(!is_char_bidi('\u{FE00}'));
        assert!(!is_char_bidi('\u{202C}'));
        assert!(is_char_bidi('\u{0590}'));
        assert!(is_char_bidi('\u{08FF}'));
        assert!(is_char_bidi('\u{061C}'));
        assert!(is_char_bidi('\u{FB50}'));
        assert!(is_char_bidi('\u{FDFF}'));
        assert!(is_char_bidi('\u{FE70}'));
        assert!(is_char_bidi('\u{FEFF}'));
        assert!(is_char_bidi('\u{200F}'));
        assert!(is_char_bidi('\u{202B}'));
        assert!(is_char_bidi('\u{202E}'));
        assert!(is_char_bidi('\u{2067}'));
        assert!(is_char_bidi('\u{10800}'));
        assert!(is_char_bidi('\u{10FFF}'));
        assert!(is_char_bidi('\u{1E800}'));
        assert!(is_char_bidi('\u{1EFFF}'));
    }

    #[test]
    fn test_is_utf16_code_unit_bidi() {
        assert!(!is_utf16_code_unit_bidi(0x0062));
        assert!(!is_utf16_code_unit_bidi(0x03B1));
        assert!(!is_utf16_code_unit_bidi(0x3041));
        assert!(!is_utf16_code_unit_bidi(0xD801));
        assert!(!is_utf16_code_unit_bidi(0xFE00));
        assert!(!is_utf16_code_unit_bidi(0x202C));
        assert!(is_utf16_code_unit_bidi(0x0590));
        assert!(is_utf16_code_unit_bidi(0x08FF));
        assert!(is_utf16_code_unit_bidi(0x061C));
        assert!(is_utf16_code_unit_bidi(0xFB50));
        assert!(is_utf16_code_unit_bidi(0xFDFF));
        assert!(is_utf16_code_unit_bidi(0xFE70));
        assert!(is_utf16_code_unit_bidi(0xFEFF));
        assert!(is_utf16_code_unit_bidi(0x200F));
        assert!(is_utf16_code_unit_bidi(0x202B));
        assert!(is_utf16_code_unit_bidi(0x202E));
        assert!(is_utf16_code_unit_bidi(0x2067));
        assert!(is_utf16_code_unit_bidi(0xD802));
        assert!(is_utf16_code_unit_bidi(0xD803));
        assert!(is_utf16_code_unit_bidi(0xD83A));
        assert!(is_utf16_code_unit_bidi(0xD83B));
    }

    #[test]
    fn test_is_str_bidi() {
        assert!(!is_str_bidi("abcdefghijklmnopaabcdefghijklmnop"));
        assert!(!is_str_bidi("abcdefghijklmnop\u{03B1}abcdefghijklmnop"));
        assert!(!is_str_bidi("abcdefghijklmnop\u{3041}abcdefghijklmnop"));
        assert!(!is_str_bidi("abcdefghijklmnop\u{1F4A9}abcdefghijklmnop"));
        assert!(!is_str_bidi("abcdefghijklmnop\u{FE00}abcdefghijklmnop"));
        assert!(!is_str_bidi("abcdefghijklmnop\u{202C}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{0590}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{08FF}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{061C}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{FB50}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{FDFF}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{FE70}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{FEFF}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{200F}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{202B}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{202E}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{2067}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{10800}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{10FFF}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{1E800}abcdefghijklmnop"));
        assert!(is_str_bidi("abcdefghijklmnop\u{1EFFF}abcdefghijklmnop"));
    }

    #[test]
    fn test_is_utf8_bidi() {
        assert!(!is_utf8_bidi(
            "abcdefghijklmnopaabcdefghijklmnop".as_bytes()
        ));
        assert!(!is_utf8_bidi(
            "abcdefghijklmnop\u{03B1}abcdefghijklmnop".as_bytes()
        ));
        assert!(!is_utf8_bidi(
            "abcdefghijklmnop\u{3041}abcdefghijklmnop".as_bytes()
        ));
        assert!(!is_utf8_bidi(
            "abcdefghijklmnop\u{1F4A9}abcdefghijklmnop".as_bytes()
        ));
        assert!(!is_utf8_bidi(
            "abcdefghijklmnop\u{FE00}abcdefghijklmnop".as_bytes()
        ));
        assert!(!is_utf8_bidi(
            "abcdefghijklmnop\u{202C}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{0590}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{08FF}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{061C}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{FB50}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{FDFF}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{FE70}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{FEFF}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{200F}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{202B}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{202E}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{2067}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{10800}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{10FFF}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{1E800}abcdefghijklmnop".as_bytes()
        ));
        assert!(is_utf8_bidi(
            "abcdefghijklmnop\u{1EFFF}abcdefghijklmnop".as_bytes()
        ));
    }

    #[test]
    fn test_is_utf16_bidi() {
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0062, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x03B1, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x3041, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD801, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFE00, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(!is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202C, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0590, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x08FF, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x061C, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFB50, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFDFF, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFE70, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFEFF, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x200F, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202B, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202E, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x2067, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD802, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD803, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD83A, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));
        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD83B, 0x62, 0x63, 0x64, 0x65, 0x66,
            0x67, 0x68, 0x69,
        ]));

        assert!(is_utf16_bidi(&[
            0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0590, 0x3041, 0x62, 0x63, 0x64, 0x65,
            0x66, 0x67, 0x68, 0x69,
        ]));
    }

    #[test]
    fn test_check_str_for_latin1_and_bidi() {
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnopaabcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{03B1}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{3041}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{1F4A9}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{FE00}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{202C}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{0590}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{08FF}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{061C}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{FB50}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{FDFF}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{FE70}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{FEFF}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{200F}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{202B}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{202E}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{2067}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{10800}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{10FFF}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{1E800}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_str_for_latin1_and_bidi("abcdefghijklmnop\u{1EFFF}abcdefghijklmnop"),
            Latin1Bidi::Bidi
        );
    }

    #[test]
    fn test_check_utf8_for_latin1_and_bidi() {
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnopaabcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{03B1}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{3041}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{1F4A9}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{FE00}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{202C}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{0590}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{08FF}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{061C}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{FB50}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{FDFF}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{FE70}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{FEFF}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{200F}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{202B}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{202E}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{2067}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{10800}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{10FFF}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{1E800}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf8_for_latin1_and_bidi("abcdefghijklmnop\u{1EFFF}abcdefghijklmnop".as_bytes()),
            Latin1Bidi::Bidi
        );
    }

    #[test]
    fn test_check_utf16_for_latin1_and_bidi() {
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0062, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x03B1, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x3041, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD801, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFE00, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_ne!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202C, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0590, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x08FF, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x061C, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFB50, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFDFF, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFE70, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xFEFF, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x200F, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202B, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x202E, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x2067, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD802, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD803, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD83A, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xD83B, 0x62, 0x63, 0x64, 0x65,
                0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );

        assert_eq!(
            check_utf16_for_latin1_and_bidi(&[
                0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x0590, 0x3041, 0x62, 0x63, 0x64,
                0x65, 0x66, 0x67, 0x68, 0x69,
            ]),
            Latin1Bidi::Bidi
        );
    }

    #[inline(always)]
    pub fn reference_is_char_bidi(c: char) -> bool {
        match c {
            '\u{0590}'...'\u{08FF}'
            | '\u{FB50}'...'\u{FDFF}'
            | '\u{FE70}'...'\u{FEFF}'
            | '\u{10800}'...'\u{10FFF}'
            | '\u{1E800}'...'\u{1EFFF}'
            | '\u{200F}'
            | '\u{202B}'
            | '\u{202E}'
            | '\u{2067}' => true,
            _ => false,
        }
    }

    #[inline(always)]
    pub fn reference_is_utf16_code_unit_bidi(u: u16) -> bool {
        match u {
            0x0590...0x08FF
            | 0xFB50...0xFDFF
            | 0xFE70...0xFEFF
            | 0xD802
            | 0xD803
            | 0xD83A
            | 0xD83B
            | 0x200F
            | 0x202B
            | 0x202E
            | 0x2067 => true,
            _ => false,
        }
    }

    #[test]
    fn test_is_char_bidi_thoroughly() {
        for i in 0..0xD800u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            assert_eq!(is_char_bidi(c), reference_is_char_bidi(c));
        }
        for i in 0xE000..0x110000u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            assert_eq!(is_char_bidi(c), reference_is_char_bidi(c));
        }
    }

    #[test]
    fn test_is_utf16_code_unit_bidi_thoroughly() {
        for i in 0..0x10000u32 {
            let u = i as u16;
            assert_eq!(
                is_utf16_code_unit_bidi(u),
                reference_is_utf16_code_unit_bidi(u)
            );
        }
    }

    #[test]
    fn test_is_str_bidi_thoroughly() {
        let mut buf = [0; 4];
        for i in 0..0xD800u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            assert_eq!(
                is_str_bidi(c.encode_utf8(&mut buf[..])),
                reference_is_char_bidi(c)
            );
        }
        for i in 0xE000..0x110000u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            assert_eq!(
                is_str_bidi(c.encode_utf8(&mut buf[..])),
                reference_is_char_bidi(c)
            );
        }
    }

    #[test]
    fn test_is_utf8_bidi_thoroughly() {
        let mut buf = [0; 8];
        for i in 0..0xD800u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            let expect = reference_is_char_bidi(c);
            {
                let len = {
                    let bytes = c.encode_utf8(&mut buf[..]).as_bytes();
                    assert_eq!(is_utf8_bidi(bytes), expect);
                    bytes.len()
                };
                {
                    let tail = &mut buf[len..];
                    for b in tail.iter_mut() {
                        *b = 0;
                    }
                }
            }
            assert_eq!(is_utf8_bidi(&buf[..]), expect);
        }
        for i in 0xE000..0x110000u32 {
            let c: char = unsafe { ::std::mem::transmute(i) };
            let expect = reference_is_char_bidi(c);
            {
                let len = {
                    let bytes = c.encode_utf8(&mut buf[..]).as_bytes();
                    assert_eq!(is_utf8_bidi(bytes), expect);
                    bytes.len()
                };
                {
                    let tail = &mut buf[len..];
                    for b in tail.iter_mut() {
                        *b = 0;
                    }
                }
            }
            assert_eq!(is_utf8_bidi(&buf[..]), expect);
        }
    }

    #[test]
    fn test_is_utf8_bidi_edge_cases() {
        assert!(!is_utf8_bidi(b"\xD5\xBF\x61"));
        assert!(!is_utf8_bidi(b"\xD6\x80\x61"));
        assert!(!is_utf8_bidi(b"abc"));
        assert!(is_utf8_bidi(b"\xD5\xBF\xC2"));
        assert!(is_utf8_bidi(b"\xD6\x80\xC2"));
        assert!(is_utf8_bidi(b"ab\xC2"));
    }
}
