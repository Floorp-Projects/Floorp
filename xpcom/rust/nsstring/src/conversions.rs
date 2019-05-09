/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

extern crate encoding_rs;

use std::slice;

use super::nsACString;
use super::nsAString;
use super::nsCStringLike;
use super::BulkWriteOk;
use super::Gecko_FallibleAssignCString;
use super::Latin1StringLike;

use conversions::encoding_rs::mem::*;
use conversions::encoding_rs::Encoding;

/// Required math stated in the docs of
/// `convert_utf16_to_utf8()`.
#[inline(always)]
fn times_three(a: usize) -> Option<usize> {
    a.checked_mul(3)
}

#[inline(always)]
fn identity(a: usize) -> Option<usize> {
    Some(a)
}

#[inline(always)]
fn plus_one(a: usize) -> Option<usize> {
    a.checked_add(1)
}

/// Typical cache line size per
/// https://stackoverflow.com/questions/14707803/line-size-of-l1-and-l2-caches
///
/// For consistent behavior, not trying to use 128 on aarch64
/// or other fanciness like that.
const CACHE_LINE: usize = 64;

const CACHE_LINE_MASK: usize = CACHE_LINE - 1;

/// Returns true if the string is both longer than a cache line
/// and the first cache line is ASCII.
#[inline(always)]
fn long_string_starts_with_ascii(buffer: &[u8]) -> bool {
    // We examine data only up to the end of the cache line
    // to make this check minimally disruptive.
    if buffer.len() <= CACHE_LINE {
        return false;
    }
    let bound = CACHE_LINE - ((buffer.as_ptr() as usize) & CACHE_LINE_MASK);
    is_ascii(&buffer[..bound])
}

/// Returns true if the string is both longer than two cache lines
/// and the first two cache lines are Basic Latin.
#[inline(always)]
fn long_string_stars_with_basic_latin(buffer: &[u16]) -> bool {
    // We look at two cache lines with code unit size of two. There is need
    // to look at more than one cache line in the UTF-16 case, because looking
    // at just one cache line wouldn't catch non-ASCII Latin with high enough
    // probability with Latin-script languages that have relatively infrequent
    // non-ASCII characters.
    if buffer.len() <= CACHE_LINE {
        return false;
    }
    let bound = (CACHE_LINE * 2 - ((buffer.as_ptr() as usize) & CACHE_LINE_MASK)) / 2;
    is_basic_latin(&buffer[..bound])
}

// Ignoring the copy avoidance complications of conversions between Latin1 and
// UTF-8, a conversion function has the outward form of
// `fn F(&mut self, other: &[T], old_len: usize) -> Result<BulkWriteOk, ()>`,
// where `T` is either `u8` or `u16`. `other` is the slice whose converted
// content are to be appended to `self` and `old_len` indicates how many
// code unit of `self` are to be preserved (0 for the assignment case and
// `self.len()` for the appending case).
//
// As implementation parameters a conversion function needs to know the
// math for computing the worst case conversion length in code units given
// the input length in code units. For a _constant conversion_ the number
// of code units the conversion produces equals the number of code units
// in the input. For a _shinking conversion_ the maximum number of code
// units the conversion can produce equals the number of code units in
// the input, but the conversion can produce fewer code units. Still, due
// to implementation details, the function might want _one_ unit more of
// output space. For an _expanding conversion_ (no need for macro), the
// minimum number of code units produced by the conversion is the number
// of code units in the input, but the conversion can produce more.
//
// Copy avoidance conversions avoid copying a refcounted buffer when it's
// ASCII-only.
//
// Internally, a conversion function needs to know the underlying
// encoding_rs conversion function, the math for computing the required
// output buffer size and, depending on the case, the underlying
// encoding_rs ASCII prefix handling function.

/// A conversion where the number of code units in the output is potentially
/// smaller than the number of code units in the input.
///
/// Takes the name of the method to be generated, the name of the conversion
/// function and the type of the input slice.
///
/// `$name` is the name of the function to generate
/// `$convert` is the underlying `encoding_rs::mem` function to use
/// `$other_ty` is the type of the input slice
/// `$math` is the worst-case length math that `$convert` expects
macro_rules! shrinking_conversion {
    (name = $name:ident,
     convert = $convert:ident,
     other_ty = $other_ty:ty,
     math = $math:ident) => (
        fn $name(&mut self, other: $other_ty, old_len: usize) -> Result<BulkWriteOk, ()> {
            let needed = $math(other.len()).ok_or(())?;
            let mut handle = unsafe {
                self.bulk_write(old_len.checked_add(needed).ok_or(())?, old_len, false)?
            };
            let written = $convert(other, &mut handle.as_mut_slice()[old_len..]);
            let new_len = old_len + written;
            Ok(handle.finish(new_len, new_len > CACHE_LINE))
        }
     )
}

/// A conversion where the number of code units in the output is always equal
/// to the number of code units in the input.
///
/// Takes the name of the method to be generated, the name of the conversion
/// function and the type of the input slice.
///
/// `$name` is the name of the function to generate
/// `$convert` is the underlying `encoding_rs::mem` function to use
/// `$other_ty` is the type of the input slice
macro_rules! constant_conversion {
    (name = $name:ident,
     convert = $convert:ident,
     other_ty = $other_ty:ty) => (
        fn $name(&mut self, other: $other_ty, old_len: usize, allow_shrinking: bool) -> Result<BulkWriteOk, ()> {
            let new_len = old_len.checked_add(other.len()).ok_or(())?;
            let mut handle = unsafe {
                self.bulk_write(new_len, old_len, allow_shrinking)?
            };
            $convert(other, &mut handle.as_mut_slice()[old_len..]);
            Ok(handle.finish(new_len, false))
        }
     )
}

/// An intermediate check for avoiding a copy and having an `nsStringBuffer`
/// refcount increment instead when both `self` and `other` are `nsACString`s,
/// `other` is entirely ASCII and all old data in `self` is discarded.
///
/// `$name` is the name of the function to generate
/// `$impl` is the underlying conversion that takes a slice and that is used
///         when we can't just adopt the incoming buffer as-is
/// `$string_like` is the kind of input taken
macro_rules! ascii_copy_avoidance {
    (name = $name:ident,
     implementation = $implementation:ident,
     string_like = $string_like:ident) => (
        fn $name<T: $string_like + ?Sized>(&mut self, other: &T, old_len: usize) -> Result<BulkWriteOk, ()> {
            let adapter = other.adapt();
            let other_slice = adapter.as_ref();
            let num_ascii = if adapter.is_abstract() && old_len == 0 {
                let up_to = Encoding::ascii_valid_up_to(other_slice);
                if up_to == other_slice.len() {
                    // Calling something whose argument can be obtained from
                    // the adapter rather than an nsStringLike avoids a huge
                    // lifetime mess by keeping nsStringLike and
                    // Latin1StringLike free of lifetime interdependencies.
                    if unsafe { Gecko_FallibleAssignCString(self, other.adapt().as_ptr()) } {
                        return Ok(BulkWriteOk{});
                    } else {
                        return Err(());
                    }
                }
                Some(up_to)
            } else {
                None
            };
            self.$implementation(other_slice, old_len, num_ascii)
        }
    )
}

impl nsAString {
    // Valid UTF-8 to UTF-16

    // Documentation says the destination buffer needs to have
    // as many code units as the input.
    shrinking_conversion!(
        name = fallible_append_str_impl,
        convert = convert_str_to_utf16,
        other_ty = &str,
        math = identity
    );

    /// Convert a valid UTF-8 string into valid UTF-16 and replace the content
    /// of this string with the conversion result.
    pub fn assign_str(&mut self, other: &str) {
        self.fallible_append_str_impl(other, 0)
            .expect("Out of memory");
    }

    /// Convert a valid UTF-8 string into valid UTF-16 and fallibly replace the
    /// content of this string with the conversion result.
    pub fn fallible_assign_str(&mut self, other: &str) -> Result<(), ()> {
        self.fallible_append_str_impl(other, 0).map(|_| ())
    }

    /// Convert a valid UTF-8 string into valid UTF-16 and append the conversion
    /// to this string.
    pub fn append_str(&mut self, other: &str) {
        let len = self.len();
        self.fallible_append_str_impl(other, len)
            .expect("Out of memory");
    }

    /// Convert a valid UTF-8 string into valid UTF-16 and fallibly append the
    /// conversion to this string.
    pub fn fallible_append_str(&mut self, other: &str) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_str_impl(other, len).map(|_| ())
    }

    // Potentially-invalid UTF-8 to UTF-16

    // Documentation says the destination buffer needs to have
    // one more code unit than the input.
    shrinking_conversion!(
        name = fallible_append_utf8_impl,
        convert = convert_utf8_to_utf16,
        other_ty = &[u8],
        math = plus_one
    );

    /// Convert a potentially-invalid UTF-8 string into valid UTF-16
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// replace the content of this string with the conversion result.
    pub fn assign_utf8(&mut self, other: &[u8]) {
        self.fallible_append_utf8_impl(other, 0)
            .expect("Out of memory");
    }

    /// Convert a potentially-invalid UTF-8 string into valid UTF-16
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// fallibly replace the content of this string with the conversion result.
    pub fn fallible_assign_utf8(&mut self, other: &[u8]) -> Result<(), ()> {
        self.fallible_append_utf8_impl(other, 0).map(|_| ())
    }

    /// Convert a potentially-invalid UTF-8 string into valid UTF-16
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// append the conversion result to this string.
    pub fn append_utf8(&mut self, other: &[u8]) {
        let len = self.len();
        self.fallible_append_utf8_impl(other, len)
            .expect("Out of memory");
    }

    /// Convert a potentially-invalid UTF-8 string into valid UTF-16
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// fallibly append the conversion result to this string.
    pub fn fallible_append_utf8(&mut self, other: &[u8]) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_utf8_impl(other, len).map(|_| ())
    }

    // Latin1 to UTF-16

    constant_conversion!(
        name = fallible_append_latin1_impl,
        convert = convert_latin1_to_utf16,
        other_ty = &[u8]
    );

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-16 and replace the content of this string with the conversion result.
    pub fn assign_latin1(&mut self, other: &[u8]) {
        self.fallible_append_latin1_impl(other, 0, true)
            .expect("Out of memory");
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-16 and fallibly replace the content of this string with the
    /// conversion result.
    pub fn fallible_assign_latin1(&mut self, other: &[u8]) -> Result<(), ()> {
        self.fallible_append_latin1_impl(other, 0, true).map(|_| ())
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-16 and append the conversion result to this string.
    pub fn append_latin1(&mut self, other: &[u8]) {
        let len = self.len();
        self.fallible_append_latin1_impl(other, len, false)
            .expect("Out of memory");
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-16 and fallibly append the conversion result to this string.
    pub fn fallible_append_latin1(&mut self, other: &[u8]) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_latin1_impl(other, len, false).map(|_| ())
    }
}

impl nsACString {
    // UTF-16 to UTF-8

    fn fallible_append_utf16_to_utf8_impl(
        &mut self,
        other: &[u16],
        old_len: usize,
    ) -> Result<BulkWriteOk, ()> {
        // We first size the buffer for ASCII if the first two cache lines are ASCII. If that turns out
        // not to be enough, we size for the worst case given the length of the remaining input at that
        // point. BUT if the worst case fits inside the inline capacity of an autostring, we skip
        // the ASCII stuff.
        let worst_case_needed = if let Some(inline_capacity) = self.inline_capacity() {
            let worst_case = times_three(other.len()).ok_or(())?;
            if worst_case <= inline_capacity {
                Some(worst_case)
            } else {
                None
            }
        } else {
            None
        };
        let (filled, read, mut handle) = if worst_case_needed.is_none() &&
                                                 long_string_stars_with_basic_latin(other) {
            let new_len_with_ascii = old_len.checked_add(other.len()).ok_or(())?;
            let mut handle = unsafe { self.bulk_write(new_len_with_ascii, old_len, false)? };
            let (read, written) = convert_utf16_to_utf8_partial(other, &mut handle.as_mut_slice()[old_len..]);
            let left = other.len() - read;
            if left == 0 {
                return Ok(handle.finish(old_len + written, true));
            }
            let filled = old_len + written;
            let needed = times_three(left).ok_or(())?;
            let new_len = filled.checked_add(needed).ok_or(())?;
            unsafe {
                handle.restart_bulk_write(new_len, filled, false)?;
            }
            (filled, read, handle)
        } else {
            // Started with non-ASCII. Compute worst case
            let needed = if let Some(n) = worst_case_needed {
                n
            } else {
                times_three(other.len()).ok_or(())?
            };
            let new_len = old_len.checked_add(needed).ok_or(())?;
            let handle = unsafe { self.bulk_write(new_len, old_len, false)? };
            (old_len, 0, handle)
        };
        let written =
            convert_utf16_to_utf8(&other[read..], &mut handle.as_mut_slice()[filled..]);
        Ok(handle.finish(filled + written, true))
    }

    /// Convert a potentially-invalid UTF-16 string into valid UTF-8
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// replace the content of this string with the conversion result.
    pub fn assign_utf16_to_utf8(&mut self, other: &[u16]) {
        self.fallible_append_utf16_to_utf8_impl(other, 0)
            .expect("Out of memory");
    }

    /// Convert a potentially-invalid UTF-16 string into valid UTF-8
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// fallibly replace the content of this string with the conversion result.
    pub fn fallible_assign_utf16_to_utf8(&mut self, other: &[u16]) -> Result<(), ()> {
        self.fallible_append_utf16_to_utf8_impl(other, 0)
            .map(|_| ())
    }

    /// Convert a potentially-invalid UTF-16 string into valid UTF-8
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// append the conversion result to this string.
    pub fn append_utf16_to_utf8(&mut self, other: &[u16]) {
        let len = self.len();
        self.fallible_append_utf16_to_utf8_impl(other, len)
            .expect("Out of memory");
    }

    /// Convert a potentially-invalid UTF-16 string into valid UTF-8
    /// (replacing invalid sequences with the REPLACEMENT CHARACTER) and
    /// fallibly append the conversion result to this string.
    pub fn fallible_append_utf16_to_utf8(&mut self, other: &[u16]) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_utf16_to_utf8_impl(other, len)
            .map(|_| ())
    }

    // UTF-16 to Latin1

    constant_conversion!(
        name = fallible_append_utf16_to_latin1_lossy_impl,
        convert = convert_utf16_to_latin1_lossy,
        other_ty = &[u16]
    );

    /// Convert a UTF-16 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// replace the content of this string with the conversion result.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-16,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn assign_utf16_to_latin1_lossy(&mut self, other: &[u16]) {
        self.fallible_append_utf16_to_latin1_lossy_impl(other, 0, true)
            .expect("Out of memory");
    }

    /// Convert a UTF-16 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// fallibly replace the content of this string with the conversion result.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-16,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn fallible_assign_utf16_to_latin1_lossy(&mut self, other: &[u16]) -> Result<(), ()> {
        self.fallible_append_utf16_to_latin1_lossy_impl(other, 0, true)
            .map(|_| ())
    }

    /// Convert a UTF-16 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// append the conversion result to this string.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-16,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn append_utf16_to_latin1_lossy(&mut self, other: &[u16]) {
        let len = self.len();
        self.fallible_append_utf16_to_latin1_lossy_impl(other, len, false)
            .expect("Out of memory");
    }

    /// Convert a UTF-16 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// fallibly append the conversion result to this string.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-16,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn fallible_append_utf16_to_latin1_lossy(&mut self, other: &[u16]) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_utf16_to_latin1_lossy_impl(other, len, false)
            .map(|_| ())
    }

    // UTF-8 to Latin1

    ascii_copy_avoidance!(
        name = fallible_append_utf8_to_latin1_lossy_check,
        implementation = fallible_append_utf8_to_latin1_lossy_impl,
        string_like = nsCStringLike
    );

    fn fallible_append_utf8_to_latin1_lossy_impl(
        &mut self,
        other: &[u8],
        old_len: usize,
        maybe_num_ascii: Option<usize>,
    ) -> Result<BulkWriteOk, ()> {
        let new_len = old_len.checked_add(other.len()).ok_or(())?;
        let num_ascii = maybe_num_ascii.unwrap_or(0);
        // Already checked for overflow above, so this can't overflow.
        let old_len_plus_num_ascii = old_len + num_ascii;
        let mut handle = unsafe { self.bulk_write(new_len, old_len, false)? };
        let written = {
            let buffer = handle.as_mut_slice();
            if num_ascii != 0 {
                (&mut buffer[old_len..old_len_plus_num_ascii]).copy_from_slice(&other[..num_ascii]);
            }
            convert_utf8_to_latin1_lossy(&other[num_ascii..], &mut buffer[old_len_plus_num_ascii..])
        };
        Ok(handle.finish(old_len_plus_num_ascii + written, true))
    }

    /// Convert a UTF-8 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// replace the content of this string with the conversion result.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-8,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn assign_utf8_to_latin1_lossy<T: nsCStringLike + ?Sized>(&mut self, other: &T) {
        self.fallible_append_utf8_to_latin1_lossy_check(other, 0)
            .expect("Out of memory");
    }

    /// Convert a UTF-8 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// fallibly replace the content of this string with the conversion result.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-8,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn fallible_assign_utf8_to_latin1_lossy<T: nsCStringLike + ?Sized>(
        &mut self,
        other: &T,
    ) -> Result<(), ()> {
        self.fallible_append_utf8_to_latin1_lossy_check(other, 0)
            .map(|_| ())
    }

    /// Convert a UTF-8 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// append the conversion result to this string.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-8,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn append_utf8_to_latin1_lossy<T: nsCStringLike + ?Sized>(&mut self, other: &T) {
        let len = self.len();
        self.fallible_append_utf8_to_latin1_lossy_check(other, len)
            .expect("Out of memory");
    }

    /// Convert a UTF-8 string whose all code points are below U+0100 into
    /// a Latin1 (scalar value is byte value; not windows-1252!) string and
    /// fallibly append the conversion result to this string.
    ///
    /// # Panics
    ///
    /// If the input contains code points above U+00FF or is not valid UTF-8,
    /// panics in debug mode and produces garbage in a memory-safe way in
    /// release builds. The nature of the garbage may differ based on CPU
    /// architecture and must not be relied upon.
    pub fn fallible_append_utf8_to_latin1_lossy<T: nsCStringLike + ?Sized>(
        &mut self,
        other: &T,
    ) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_utf8_to_latin1_lossy_check(other, len)
            .map(|_| ())
    }

    // Latin1 to UTF-8 CString

    ascii_copy_avoidance!(
        name = fallible_append_latin1_to_utf8_check,
        implementation = fallible_append_latin1_to_utf8_impl,
        string_like = Latin1StringLike
    );

    fn fallible_append_latin1_to_utf8_impl(
        &mut self,
        other: &[u8],
        old_len: usize,
        maybe_num_ascii: Option<usize>,
    ) -> Result<BulkWriteOk, ()> {
        let (filled, read, mut handle) = if let Some(num_ascii) = maybe_num_ascii {
            // Wrapper checked for ASCII
            let left = other.len() - num_ascii;
            let filled = old_len + num_ascii;
            let needed = left.checked_mul(2).ok_or(())?;
            let new_len = filled.checked_add(needed).ok_or(())?;
            let mut handle = unsafe { self.bulk_write(new_len, old_len, false)? };
            if num_ascii != 0 {
                (&mut handle.as_mut_slice()[old_len..filled]).copy_from_slice(&other[..num_ascii]);
            }
            (filled, num_ascii, handle)
        } else {
            let worst_case_needed = if let Some(inline_capacity) = self.inline_capacity() {
                let worst_case = other.len().checked_mul(2).ok_or(())?;
                if worst_case <= inline_capacity {
                    Some(worst_case)
                } else {
                    None
                }
            } else {
                None
            };
            if worst_case_needed.is_none() && long_string_starts_with_ascii(other) {
                // Wrapper didn't check for ASCII, so let's see if `other` starts with ASCII
                // `other` starts with ASCII, so let's first size the buffer
                // with optimism that it's ASCII-only.
                let new_len_with_ascii = old_len.checked_add(other.len()).ok_or(())?;
                let mut handle = unsafe { self.bulk_write(new_len_with_ascii, old_len, false)? };
                let (read, written) = convert_latin1_to_utf8_partial(other, &mut handle.as_mut_slice()[old_len..]);
                let left = other.len() - read;
                let filled = old_len + written;
                if left == 0 {
                    // `other` fit in the initial allocation
                    return Ok(handle.finish(filled, true));
                }
                let needed = left.checked_mul(2).ok_or(())?;
                let new_len = filled.checked_add(needed).ok_or(())?;
                unsafe {
                    handle.restart_bulk_write(new_len, filled, false)?;
                }
                (filled, read, handle)
            } else {
                // Started with non-ASCII. Assume worst case.
                let needed = if let Some(n) = worst_case_needed {
                    n
                } else {
                    other.len().checked_mul(2).ok_or(())?
                };
                let new_len = old_len.checked_add(needed).ok_or(())?;
                let handle = unsafe { self.bulk_write(new_len, old_len, false)? };
                (old_len, 0, handle)
            }
        };
        let written =
            convert_latin1_to_utf8(&other[read..], &mut handle.as_mut_slice()[filled..]);
        Ok(handle.finish(filled + written, true))
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-8 and replace the content of this string with the conversion result.
    pub fn assign_latin1_to_utf8<T: Latin1StringLike + ?Sized>(&mut self, other: &T) {
        self.fallible_append_latin1_to_utf8_check(other, 0)
            .expect("Out of memory");
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-8 and fallibly replace the content of this string with the
    /// conversion result.
    pub fn fallible_assign_latin1_to_utf8<T: Latin1StringLike + ?Sized>(
        &mut self,
        other: &T,
    ) -> Result<(), ()> {
        self.fallible_append_latin1_to_utf8_check(other, 0)
            .map(|_| ())
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-8 and append the conversion result to this string.
    pub fn append_latin1_to_utf8<T: Latin1StringLike + ?Sized>(&mut self, other: &T) {
        let len = self.len();
        self.fallible_append_latin1_to_utf8_check(other, len)
            .expect("Out of memory");
    }

    /// Convert a Latin1 (i.e. byte value equals scalar value; not windows-1252!)
    /// into UTF-8 and fallibly append the conversion result to this string.
    pub fn fallible_append_latin1_to_utf8<T: Latin1StringLike + ?Sized>(
        &mut self,
        other: &T,
    ) -> Result<(), ()> {
        let len = self.len();
        self.fallible_append_latin1_to_utf8_check(other, len)
            .map(|_| ())
    }
}

#[no_mangle]
pub unsafe extern "C" fn nsstring_fallible_append_utf8_impl(
    this: *mut nsAString,
    other: *const u8,
    other_len: usize,
    old_len: usize,
) -> bool {
    let other_slice = slice::from_raw_parts(other, other_len);
    (*this)
        .fallible_append_utf8_impl(other_slice, old_len)
        .is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn nsstring_fallible_append_latin1_impl(
    this: *mut nsAString,
    other: *const u8,
    other_len: usize,
    old_len: usize,
    allow_shrinking: bool,
) -> bool {
    let other_slice = slice::from_raw_parts(other, other_len);
    (*this)
        .fallible_append_latin1_impl(other_slice, old_len, allow_shrinking)
        .is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn nscstring_fallible_append_utf16_to_utf8_impl(
    this: *mut nsACString,
    other: *const u16,
    other_len: usize,
    old_len: usize,
) -> bool {
    let other_slice = slice::from_raw_parts(other, other_len);
    (*this)
        .fallible_append_utf16_to_utf8_impl(other_slice, old_len)
        .is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn nscstring_fallible_append_utf16_to_latin1_lossy_impl(
    this: *mut nsACString,
    other: *const u16,
    other_len: usize,
    old_len: usize,
    allow_shrinking: bool,
) -> bool {
    let other_slice = slice::from_raw_parts(other, other_len);
    (*this)
        .fallible_append_utf16_to_latin1_lossy_impl(other_slice, old_len, allow_shrinking)
        .is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn nscstring_fallible_append_utf8_to_latin1_lossy_check(
    this: *mut nsACString,
    other: *const nsACString,
    old_len: usize,
) -> bool {
    (*this)
        .fallible_append_utf8_to_latin1_lossy_check(&*other, old_len)
        .is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn nscstring_fallible_append_latin1_to_utf8_check(
    this: *mut nsACString,
    other: *const nsACString,
    old_len: usize,
) -> bool {
    (*this)
        .fallible_append_latin1_to_utf8_check(&*other, old_len)
        .is_ok()
}
