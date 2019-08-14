// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#ifndef encoding_rs_mem_h_
#define encoding_rs_mem_h_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * _Note:_ "Latin1" in this header refers to the Unicode range from U+0000 to
 * U+00FF, inclusive, and does not refer to the windows-1252 range. This
 * in-memory encoding is sometimes used as a storage optimization of text
 * when UTF-16 indexing and length semantics are exposed.
 */

/**
 * Classification of text as Latin1 (all code points are below U+0100),
 * left-to-right with some non-Latin1 characters or as containing at least
 * some right-to-left characters.
 */
typedef enum {
  /**
   * Every character is below U+0100.
   */
  Latin1 = 0,
  /**
   * There is at least one character that's U+0100 or higher, but there
   * are no right-to-left characters.
   */
  LeftToRight = 1,
  /**
   * There is at least one right-to-left character.
   */
  Bidi = 2,
} Latin1Bidi;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Checks whether a valid UTF-8 buffer contains code points
 * that trigger right-to-left processing or is all-Latin1.
 *
 * Possibly more efficient than performing the checks separately.
 *
 * Returns `Latin1Bidi::Latin1` if `is_str_latin1()` would return `true`.
 * Otherwise, returns `Latin1Bidi::Bidi` if `is_str_bidi()` would return
 * `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
 * if `buffer` is `NULL`, or if the memory designated by `buffer` and
 * `buffer_len` does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer`
 * may be bogus but still has to be non-`NULL`.)
 */
Latin1Bidi encoding_mem_check_str_for_latin1_and_bidi(const char* buffer,
                                                      size_t len);

/**
 * Checks whether a potentially invalid UTF-16 buffer contains code points
 * that trigger right-to-left processing or is all-Latin1.
 *
 * Possibly more efficient than performing the checks separately.
 *
 * Returns `Latin1Bidi::Latin1` if `is_utf16_latin1()` would return `true`.
 * Otherwise, returns `Latin1Bidi::Bidi` if `is_utf16_bidi()` would return
 * `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
Latin1Bidi encoding_mem_check_utf16_for_latin1_and_bidi(const char16_t* buffer,
                                                        size_t len);

/**
 * Checks whether a potentially invalid UTF-8 buffer contains code points
 * that trigger right-to-left processing or is all-Latin1.
 *
 * Possibly more efficient than performing the checks separately.
 *
 * Returns `Latin1Bidi::Latin1` if `is_utf8_latin1()` would return `true`.
 *
 * Otherwise, returns `Latin1Bidi::Bidi` if `is_utf8_bidi()` would return
 * `true`. Otherwise, returns `Latin1Bidi::LeftToRight`.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL`.)
 */
Latin1Bidi encoding_mem_check_utf8_for_latin1_and_bidi(const char* buffer,
                                                       size_t len);

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-16.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * The number of `char16_t`s written equals the length of the source buffer.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
void encoding_mem_convert_latin1_to_utf16(const char* src, size_t src_len,
                                          char16_t* dst, size_t dst_len);

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-8.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer times two.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Safety
 *
 * Note that this function may write garbage beyond the number of bytes
 * indicated by the return value.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_convert_latin1_to_utf8(const char* src, size_t src_len,
                                           char* dst, size_t dst_len);

/**
 * Converts bytes whose unsigned value is interpreted as Unicode code point
 * (i.e. U+0000 to U+00FF, inclusive) to UTF-8 with potentially insufficient
 * output space.
 *
 * Returns the number of bytes read and the number of bytes written.
 *
 * If the output isn't large enough, not all input is consumed.
 *
 * # Safety
 *
 * Note that this function may write garbage beyond the number of bytes
 * indicated by the return value, so using a `&mut str` interpreted as
 * `&mut [u8]` as the destination is not safe. If you want to convert into
 * a `&mut str`, use `convert_utf16_to_str()` instead of this function.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
void encoding_mem_convert_latin1_to_utf8_partial(const char* src,
                                                 size_t* src_len, char* dst,
                                                 size_t* dst_len);

/**
 * Converts valid UTF-8 to valid UTF-16.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL`, if the two memory blocks overlap, of if the
 * buffer designated by `src` and `src_len` does not contain valid UTF-8. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_convert_str_to_utf16(const char* src, size_t src_len,
                                         char16_t* dst, size_t dst_len);

/**
 * If the input is valid UTF-16 representing only Unicode code points from
 * U+0000 to U+00FF, inclusive, converts the input into output that
 * represents the value of each code point as the unsigned byte value of
 * each output byte.
 *
 * If the input does not fulfill the condition stated above, does something
 * that is memory-safe without any promises about any properties of the
 * output and will probably assert in debug builds in future versions.
 * In particular, callers shouldn't assume the output to be the same across
 * crate versions or CPU architectures and should not assume that non-ASCII
 * input can't map to ASCII output.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * The number of bytes written equals the length of the source buffer.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 * (Probably in future versions if debug assertions are enabled (and not
 * fuzzing) and the input is not in the range U+0000 to U+00FF, inclusive.)
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
void encoding_mem_convert_utf16_to_latin1_lossy(const char16_t* src,
                                                size_t src_len, char* dst,
                                                size_t dst_len);

/**
 * Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
 * with the REPLACEMENT CHARACTER.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer times three.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_convert_utf16_to_utf8(const char16_t* src, size_t src_len,
                                          char* dst, size_t dst_len);

/**
 * Converts potentially-invalid UTF-16 to valid UTF-8 with errors replaced
 * with the REPLACEMENT CHARACTER with potentially insufficient output
 * space.
 *
 * Returns the number of code units read and the number of bytes written.
 *
 * Guarantees that the bytes in the destination beyond the number of
 * bytes claimed as written by the second item of the return tuple
 * are left unmodified.
 *
 * Not all code units are read if there isn't enough output space.
 * Note  that this method isn't designed for general streamability but for
 * not allocating memory for the worst case up front. Specifically,
 * if the input starts with or ends with an unpaired surrogate, those are
 * replaced with the REPLACEMENT CHARACTER.
 *
 * Matches the semantics of `TextEncoder.encodeInto()` from the
 * Encoding Standard.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
void encoding_mem_convert_utf16_to_utf8_partial(const char16_t* src,
                                                size_t* src_len, char* dst,
                                                size_t* dst_len);

/**
 * If the input is valid UTF-8 representing only Unicode code points from
 * U+0000 to U+00FF, inclusive, converts the input into output that
 * represents the value of each code point as the unsigned byte value of
 * each output byte.
 *
 * If the input does not fulfill the condition stated above, this function
 * panics if debug assertions are enabled (and fuzzing isn't) and otherwise
 * does something that is memory-safe without any promises about any
 * properties of the output. In particular, callers shouldn't assume the
 * output to be the same across crate versions or CPU architectures and
 * should not assume that non-ASCII input can't map to ASCII output.
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 * If debug assertions are enabled (and not fuzzing) and the input is
 * not in the range U+0000 to U+00FF, inclusive.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_convert_utf8_to_latin1_lossy(const char* src,
                                                 size_t src_len, char* dst,
                                                 size_t dst_len);

/**
 * Converts potentially-invalid UTF-8 to valid UTF-16 with errors replaced
 * with the REPLACEMENT CHARACTER.
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer _plus one_.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_convert_utf8_to_utf16(const char* src, size_t src_len,
                                          char16_t* dst, size_t dst_len);

/**
 * Copies ASCII from source to destination up to the first non-ASCII byte
 * (or the end of the input if it is ASCII in its entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_copy_ascii_to_ascii(const char* src, size_t src_len,
                                        char* dst, size_t dst_len);

/**
 * Copies ASCII from source to destination zero-extending it to UTF-16 up to
 * the first non-ASCII byte (or the end of the input if it is ASCII in its
 * entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of `char16_t`s written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_copy_ascii_to_basic_latin(const char* src, size_t src_len,
                                              char16_t* dst, size_t dst_len);

/**
 * Copies Basic Latin from source to destination narrowing it to ASCII up to
 * the first non-Basic Latin code unit (or the end of the input if it is
 * Basic Latin in its entirety).
 *
 * The length of the destination buffer must be at least the length of the
 * source buffer.
 *
 * Returns the number of bytes written.
 *
 * # Panics
 *
 * Panics if the destination buffer is shorter than stated above.
 *
 * # Undefined behavior
 *
 * UB ensues if `src` and `src_len` don't designate a valid memory block, if
 * `src` is `NULL`, if `dst` and `dst_len` don't designate a valid memory
 * block, if `dst` is `NULL` or if the two memory blocks overlap. (If
 * `src_len` is `0`, `src` may be bogus but still has to be non-`NULL` and
 * aligned. Likewise for `dst` and `dst_len`.)
 */
size_t encoding_mem_copy_basic_latin_to_ascii(const char16_t* src,
                                              size_t src_len, char* dst,
                                              size_t dst_len);

/**
 * Replaces unpaired surrogates in the input with the REPLACEMENT CHARACTER.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
void encoding_mem_ensure_utf16_validity(char16_t* buffer, size_t len);

/**
 * Checks whether the buffer is all-ASCII.
 *
 * May read the entire buffer even if it isn't all-ASCII. (I.e. the function
 * is not guaranteed to fail fast.)
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL`.)
 */
bool encoding_mem_is_ascii(const char* buffer, size_t len);

/**
 * Checks whether the buffer is all-Basic Latin (i.e. UTF-16 representing
 * only ASCII characters).
 *
 * May read the entire buffer even if it isn't all-ASCII. (I.e. the function
 * is not guaranteed to fail fast.)
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
bool encoding_mem_is_basic_latin(const char16_t* buffer, size_t len);

/**
 * Checks whether a scalar value triggers right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 *
 * # Undefined behavior
 *
 * Undefined behavior ensues if `c` is not a valid Unicode Scalar Value.
 */
bool encoding_mem_is_char_bidi(char32_t c);

/**
 * Checks whether a valid UTF-8 buffer contains code points that trigger
 * right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
 * if `buffer` is `NULL`, or if the memory designated by `buffer` and
 * `buffer_len` does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer`
 * may be bogus but still has to be non-`NULL`.)
 */
bool encoding_mem_is_str_bidi(const char* buffer, size_t len);

/**
 * Checks whether the buffer represents only code point less than or equal
 * to U+00FF.
 *
 * Fails fast. (I.e. returns before having read the whole buffer if code
 * points above U+00FF are discovered.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block,
 * if `buffer` is `NULL`, or if the memory designated by `buffer` and
 * `buffer_len` does not contain valid UTF-8. (If `buffer_len` is `0`, `buffer`
 * may be bogus but still has to be non-`NULL`.)
 */
bool encoding_mem_is_str_latin1(const char* buffer, size_t len);

/**
 * Checks whether a UTF-16 buffer contains code points that trigger
 * right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Returns `true` if the input contains an RTL character or an unpaired
 * high surrogate that could be the high half of an RTL character.
 * Returns `false` if the input contains neither RTL characters nor
 * unpaired high surrogates that could be higher halves of RTL characters.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
bool encoding_mem_is_utf16_bidi(const char16_t* buffer, size_t len);

/**
 * Checks whether a UTF-16 code unit triggers right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Since supplementary-plane right-to-left blocks are identifiable from the
 * high surrogate without examining the low surrogate, this function returns
 * `true` for such high surrogates making the function suitable for handling
 * supplementary-plane text without decoding surrogate pairs to scalar
 * values. Obviously, such high surrogates are then reported as right-to-left
 * even if actually unpaired.
 */
bool encoding_mem_is_utf16_code_unit_bidi(char16_t u);

/**
 * Checks whether the buffer represents only code point less than or equal
 * to U+00FF.
 *
 * May read the entire buffer even if it isn't all-Latin1. (I.e. the function
 * is not guaranteed to fail fast.)
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
bool encoding_mem_is_utf16_latin1(const char16_t* buffer, size_t len);

/**
 * Checks whether a potentially-invalid UTF-8 buffer contains code points
 * that trigger right-to-left processing.
 *
 * The check is done on a Unicode block basis without regard to assigned
 * vs. unassigned code points in the block. Hebrew presentation forms in
 * the Alphabetic Presentation Forms block are treated as if they formed
 * a block on their own (i.e. it treated as right-to-left). Additionally,
 * the four RIGHT-TO-LEFT FOO controls in General Punctuation are checked
 * for. Control characters that are technically bidi controls but do not
 * cause right-to-left behavior without the presence of right-to-left
 * characters or right-to-left controls are not checked for. As a special
 * case, U+FEFF is excluded from Arabic Presentation Forms-B.
 * Returns `true` if the input is invalid UTF-8 or the input contains an
 * RTL character. Returns `false` if the input is valid UTF-8 and contains
 * no RTL characters.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL`.)
 */
bool encoding_mem_is_utf8_bidi(const char* buffer, size_t len);

/**
 * Checks whether the buffer is valid UTF-8 representing only code points
 * less than or equal to U+00FF.
 *
 * Fails fast. (I.e. returns before having read the whole buffer if UTF-8
 * invalidity or code points above U+00FF are discovered.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL`.)
 */
bool encoding_mem_is_utf8_latin1(const char* buffer, size_t len);

/**
 * Returns the index of the first unpaired surrogate or, if the input is
 * valid UTF-16 in its entirety, the length of the input.
 *
 * # Undefined behavior
 *
 * UB ensues if `buffer` and `buffer_len` don't designate a valid memory block
 * or if `buffer` is `NULL`. (If `buffer_len` is `0`, `buffer` may be bogus but
 * still has to be non-`NULL` and aligned.)
 */
size_t encoding_mem_utf16_valid_up_to(const char16_t* buffer, size_t len);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // encoding_rs_mem_h_
