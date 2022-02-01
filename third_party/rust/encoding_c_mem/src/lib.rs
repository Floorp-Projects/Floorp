// Copyright Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! FFI bindings for `encoding_rs::mem`.
//!
//! _Note:_ "Latin1" in this module refers to the Unicode range from U+0000 to
//! U+00FF, inclusive, and does not refer to the windows-1252 range. This
//! in-memory encoding is sometimes used as a storage optimization of text
//! when UTF-16 indexing and length semantics are exposed.

use encoding_rs::mem::Latin1Bidi;

/// Checks whether the buffer is all-ASCII.
///
/// May read the entire buffer even if it isn't all-ASCII. (I.e. the function
/// is not guaranteed to fail fast.)
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_ascii(buffer: *const u8, len: usize) -> bool {
    encoding_rs::mem::is_ascii(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether the buffer is all-Basic Latin (i.e. UTF-16 representing
/// only ASCII characters).
///
/// May read the entire buffer even if it isn't all-ASCII. (I.e. the function
/// is not guaranteed to fail fast.)
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_basic_latin(buffer: *const u16, len: usize) -> bool {
    encoding_rs::mem::is_basic_latin(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether the buffer is valid UTF-8 representing only code points
/// less than or equal to U+00FF.
///
/// Fails fast. (I.e. returns before having read the whole buffer if UTF-8
/// invalidity or code points above U+00FF are discovered.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_utf8_latin1(buffer: *const u8, len: usize) -> bool {
    encoding_rs::mem::is_utf8_latin1(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether the buffer represents only code points less than or equal
/// to U+00FF.
///
/// Fails fast. (I.e. returns before having read the whole buffer if code
/// points above U+00FF are discovered.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
/// if `buffer` is `NULL`, or if the memory designated by `buffer` and `buffer_len`
/// does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_str_latin1(buffer: *const u8, len: usize) -> bool {
    encoding_rs::mem::is_str_latin1(::std::str::from_utf8_unchecked(
        ::std::slice::from_raw_parts(buffer, len),
    ))
}

/// Checks whether the buffer represents only code point less than or equal
/// to U+00FF.
///
/// May read the entire buffer even if it isn't all-Latin1. (I.e. the function
/// is not guaranteed to fail fast.)
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_utf16_latin1(buffer: *const u16, len: usize) -> bool {
    encoding_rs::mem::is_utf16_latin1(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether a potentially-invalid UTF-8 buffer contains code points
/// that trigger right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Hebrew presentation forms in
/// the Alphabetic Presentation Forms block are treated as if they formed
/// a block on their own (i.e. it treated as right-to-left). Additionally,
/// the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
/// for. Control characters that are technically bidi controls but do not
/// cause right-to-left behavior without the presence of right-to-left
/// characters or right-to-left controls are not checked for. As a special
/// case, U+FEFF is excluded from Arabic Presentation Forms-B.
///
/// Returns `true` if the input is invalid UTF-8 or the input contains an
/// RTL character. Returns `false` if the input is valid UTF-8 and contains
/// no RTL characters.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_utf8_bidi(buffer: *const u8, len: usize) -> bool {
    encoding_rs::mem::is_utf8_bidi(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether a valid UTF-8 buffer contains code points that trigger
/// right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Hebrew presentation forms in
/// the Alphabetic Presentation Forms block are treated as if they formed
/// a block on their own (i.e. it treated as right-to-left). Additionally,
/// the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
/// for. Control characters that are technically bidi controls but do not
/// cause right-to-left behavior without the presence of right-to-left
/// characters or right-to-left controls are not checked for. As a special
/// case, U+FEFF is excluded from Arabic Presentation Forms-B.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
/// if `buffer` is `NULL`, or if the memory designated by `buffer` and `buffer_len`
/// does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_str_bidi(buffer: *const u8, len: usize) -> bool {
    encoding_rs::mem::is_str_bidi(::std::str::from_utf8_unchecked(
        ::std::slice::from_raw_parts(buffer, len),
    ))
}

/// Checks whether a UTF-16 buffer contains code points that trigger
/// right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Hebrew presentation forms in
/// the Alphabetic Presentation Forms block are treated as if they formed
/// a block on their own (i.e. it treated as right-to-left). Additionally,
/// the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
/// for. Control characters that are technically bidi controls but do not
/// cause right-to-left behavior without the presence of right-to-left
/// characters or right-to-left controls are not checked for. As a special
/// case, U+FEFF is excluded from Arabic Presentation Forms-B.
///
/// Returns `true` if the input contains an RTL character or an unpaired
/// high surrogate that could be the high half of an RTL character.
/// Returns `false` if the input contains neither RTL characters nor
/// unpaired high surrogates that could be higher halves of RTL characters.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_utf16_bidi(buffer: *const u16, len: usize) -> bool {
    encoding_rs::mem::is_utf16_bidi(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether a scalar value triggers right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Hebrew presentation forms in
/// the Alphabetic Presentation Forms block are treated as if they formed
/// a block on their own (i.e. it treated as right-to-left). Additionally,
/// the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
/// for. Control characters that are technically bidi controls but do not
/// cause right-to-left behavior without the presence of right-to-left
/// characters or right-to-left controls are not checked for. As a special
/// case, U+FEFF is excluded from Arabic Presentation Forms-B.
///
/// # Undefined behavior
///
/// Undefined behavior ensues if `c` is not a valid Unicode Scalar Value.
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_char_bidi(c: char) -> bool {
    encoding_rs::mem::is_char_bidi(c)
}

/// Checks whether a UTF-16 code unit triggers right-to-left processing.
///
/// The check is done on a Unicode block basis without regard to assigned
/// vs. unassigned code points in the block. Hebrew presentation forms in
/// the Alphabetic Presentation Forms block are treated as if they formed
/// a block on their own (i.e. it treated as right-to-left). Additionally,
/// the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
/// for. Control characters that are technically bidi controls but do not
/// cause right-to-left behavior without the presence of right-to-left
/// characters or right-to-left controls are not checked for. As a special
/// case, U+FEFF is excluded from Arabic Presentation Forms-B.
///
/// Since supplementary-plane right-to-left blocks are identifiable from the
/// high surrogate without examining the low surrogate, this function returns
/// `true` for such high surrogates making the function suitable for handling
/// supplementary-plane text without decoding surrogate pairs to scalar
/// values. Obviously, such high surrogates are then reported as right-to-left
/// even if actually unpaired.
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_is_utf16_code_unit_bidi(u: u16) -> bool {
    encoding_rs::mem::is_utf16_code_unit_bidi(u)
}

/// Checks whether a potentially invalid UTF-8 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_utf8_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_utf8_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_check_utf8_for_latin1_and_bidi(
    buffer: *const u8,
    len: usize,
) -> Latin1Bidi {
    encoding_rs::mem::check_utf8_for_latin1_and_bidi(::std::slice::from_raw_parts(buffer, len))
}

/// Checks whether a valid UTF-8 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_str_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_str_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
/// if `buffer` is `NULL`, or if the memory designated by `buffer` and `buffer_len`
/// does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_check_str_for_latin1_and_bidi(
    buffer: *const u8,
    len: usize,
) -> Latin1Bidi {
    encoding_rs::mem::check_str_for_latin1_and_bidi(::std::str::from_utf8_unchecked(
        ::std::slice::from_raw_parts(buffer, len),
    ))
}

/// Checks whether a potentially invalid UTF-16 buffer contains code points
/// that trigger right-to-left processing or is all-Latin1.
///
/// Possibly more efficient than performing the checks separately.
///
/// Returns `Latin1Bidi::Latin1` if `is_utf16_latin1()` would return `true`.
/// Otherwise, returns `Latin1Bidi::Bidi` if `is_utf16_bidi()` would return
/// `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_check_utf16_for_latin1_and_bidi(
    buffer: *const u16,
    len: usize,
) -> Latin1Bidi {
    encoding_rs::mem::check_utf16_for_latin1_and_bidi(::std::slice::from_raw_parts(buffer, len))
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf8_to_utf16(
    src: *const u8,
    src_len: usize,
    dst: *mut u16,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_utf8_to_utf16(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL`, if the two memory blocks overlap, of if the
/// buffer designated by `src` and `src_len` does not contain valid UTF-8. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_str_to_utf16(
    src: *const u8,
    src_len: usize,
    dst: *mut u16,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_str_to_utf16(
        ::std::str::from_utf8_unchecked(::std::slice::from_raw_parts(src, src_len)),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
}

/// Converts potentially-invalid UTF-8 to valid UTF-16 signaling on error.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// Returns the number of `u16`s written or `SIZE_MAX` if the input was invalid.
///
/// When the input was invalid, some output may have been written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf8_to_utf16_without_replacement(
    src: *const u8,
    src_len: usize,
    dst: *mut u16,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_utf8_to_utf16_without_replacement(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    ).unwrap_or(::std::usize::MAX)
}

/// Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
/// with the REPLACEMENT CHARACTER with potentially insufficient output
/// space.
///
/// Writes the number of code units read into `*src_len` and the number of
/// bytes written into `*dst_len`.
///
/// Guarantees that the bytes in the destination beyond the number of
/// bytes claimed as written by the second item of the return tuple
/// are left unmodified.
///
/// Not all code units are read if there isn't enough output space.
///
/// Note  that this method isn't designed for general streamability but for
/// not allocating memory for the worst case up front. Specifically,
/// if the input starts with or ends with an unpaired surrogate, those are
/// replaced with the REPLACEMENT CHARACTER.
///
/// Matches the semantics of `TextEncoder.encodeInto()` from the
/// Encoding Standard.
///
/// # Safety
///
/// If you want to convert into a `&mut str`, use
/// `convert_utf16_to_str_partial()` instead of using this function
/// together with the `unsafe` method `as_bytes_mut()` on `&mut str`.
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf16_to_utf8_partial(
    src: *const u16,
    src_len: *mut usize,
    dst: *mut u8,
    dst_len: *mut usize,
) {
    let (read, written) = encoding_rs::mem::convert_utf16_to_utf8_partial(
        ::std::slice::from_raw_parts(src, *src_len),
        ::std::slice::from_raw_parts_mut(dst, *dst_len),
    );
    *src_len = read;
    *dst_len = written;
}

/// Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
/// with the REPLACEMENT CHARACTER.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer times three.
///
/// Returns the number of bytes written.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
///
/// # Safety
///
/// If you want to convert into a `&mut str`, use `convert_utf16_to_str()`
/// instead of using this function together with the `unsafe` method
/// `as_bytes_mut()` on `&mut str`.
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf16_to_utf8(
    src: *const u16,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_utf16_to_utf8(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_latin1_to_utf16(
    src: *const u8,
    src_len: usize,
    dst: *mut u16,
    dst_len: usize,
) {
    encoding_rs::mem::convert_latin1_to_utf16(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    );
}

/// Converts bytes whose unsigned value is interpreted as Unicode code point
/// (i.e. U+0000 to U+00FF, inclusive) to UTF-8 with potentially insufficient
/// output space.
///
/// Writes the number of code units read into `*src_len` and the number of
/// bytes written into `*dst_len`.
///
/// If the output isn't large enough, not all input is consumed.
///
/// # Safety
///
/// If you want to convert into a `&mut str`, use
/// `encoding_mem_convert_latin1_to_str_partial()` instead of using this function
/// together with the `unsafe` method `as_bytes_mut()` on `&mut str`.
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_latin1_to_utf8_partial(
    src: *const u8,
    src_len: *mut usize,
    dst: *mut u8,
    dst_len: *mut usize,
) {
    let (read, written) = encoding_rs::mem::convert_latin1_to_utf8_partial(
        ::std::slice::from_raw_parts(src, *src_len),
        ::std::slice::from_raw_parts_mut(dst, *dst_len),
    );
    *src_len = read;
    *dst_len = written;
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_latin1_to_utf8(
    src: *const u8,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_latin1_to_utf8(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
}

/// If the input is valid UTF-8 representing only Unicode code points from
/// U+0000 to U+00FF, inclusive, converts the input into output that
/// represents the value of each code point as the unsigned byte value of
/// each output byte.
///
/// If the input does not fulfill the condition stated above, this function
/// panics if debug assertions are enabled (and fuzzing isn't) and otherwise
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
///
/// If debug assertions are enabled (and not fuzzing) and the input is
/// not in the range U+0000 to U+00FF, inclusive.
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf8_to_latin1_lossy(
    src: *const u8,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::convert_utf8_to_latin1_lossy(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
}

/// If the input is valid UTF-16 representing only Unicode code points from
/// U+0000 to U+00FF, inclusive, converts the input into output that
/// represents the value of each code point as the unsigned byte value of
/// each output byte.
///
/// If the input does not fulfill the condition stated above, does something
/// that is memory-safe without any promises about any properties of the
/// output and will probably assert in debug builds in future versions.
/// In particular, callers shouldn't assume the output to be the same across
/// crate versions or CPU architectures and should not assume that non-ASCII
/// input can't map to ASCII output.
///
/// The length of the destination buffer must be at least the length of the
/// source buffer.
///
/// The number of bytes written equals the length of the source buffer.
///
/// # Panics
///
/// Panics if the destination buffer is shorter than stated above.
///
/// (Probably in future versions if debug assertions are enabled (and not
/// fuzzing) and the input is not in the range U+0000 to U+00FF, inclusive.)
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_convert_utf16_to_latin1_lossy(
    src: *const u16,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) {
    encoding_rs::mem::convert_utf16_to_latin1_lossy(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    );
}

/// Returns the index of the first unpaired surrogate or, if the input is
/// valid UTF-16 in its entirety, the length of the input.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_utf16_valid_up_to(buffer: *const u16, len: usize) -> usize {
    encoding_rs::mem::utf16_valid_up_to(::std::slice::from_raw_parts(buffer, len))
}

/// Returns the index of first byte that starts an invalid byte
/// sequence or a non-Latin1 byte sequence, or the length of the
/// string if there are neither.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_utf8_latin1_up_to(buffer: *const u8, len: usize) -> usize {
    encoding_rs::mem::utf8_latin1_up_to(::std::slice::from_raw_parts(buffer, len))
}

/// Returns the index of first byte that starts a non-Latin1 byte
/// sequence, or the length of the string if there are none.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
/// if `buffer` is `NULL`, or if the memory block does not contain valid UTF-8.
/// (If `buffer_len` is `0`, `buffer` may be bogus but still has to be non-`NULL`
/// and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_str_latin1_up_to(buffer: *const u8, len: usize) -> usize {
    encoding_rs::mem::str_latin1_up_to(::std::str::from_utf8_unchecked(
        ::std::slice::from_raw_parts(buffer, len),
    ))
}

/// Replaces unpaired surrogates in the input with the REPLACEMENT CHARACTER.
///
/// # Undefined behavior
///
/// UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
/// or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
/// still has to be non-`NULL` and aligned.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_ensure_utf16_validity(buffer: *mut u16, len: usize) {
    encoding_rs::mem::ensure_utf16_validity(::std::slice::from_raw_parts_mut(buffer, len));
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_copy_ascii_to_ascii(
    src: *const u8,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::copy_ascii_to_ascii(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_copy_ascii_to_basic_latin(
    src: *const u8,
    src_len: usize,
    dst: *mut u16,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::copy_ascii_to_basic_latin(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
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
///
/// # Undefined behavior
///
/// UB ensues if `src` and `src_len` don't designate a valid memory block, if
/// `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
/// block, if `dst` is `NULL` or if the two memory blocks overlap. (If
/// `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
/// aligned. Likewise for `dst` and `dst_len`.)
#[no_mangle]
pub unsafe extern "C" fn encoding_mem_copy_basic_latin_to_ascii(
    src: *const u16,
    src_len: usize,
    dst: *mut u8,
    dst_len: usize,
) -> usize {
    encoding_rs::mem::copy_basic_latin_to_ascii(
        ::std::slice::from_raw_parts(src, src_len),
        ::std::slice::from_raw_parts_mut(dst, dst_len),
    )
}
