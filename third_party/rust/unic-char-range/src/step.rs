// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::char;

pub const BEFORE_SURROGATE: char = '\u{D7FF}';
pub const AFTER_SURROGATE: char = '\u{E000}';

#[inline]
#[allow(unsafe_code)]
/// Step a character one step towards `char::MAX`.
///
/// # Safety
///
/// If the given character is `char::MAX`, the return value is not a valid character.
pub unsafe fn forward(ch: char) -> char {
    if ch == BEFORE_SURROGATE {
        AFTER_SURROGATE
    } else {
        char::from_u32_unchecked(ch as u32 + 1)
    }
}

#[inline]
#[allow(unsafe_code)]
/// Step a character one step towards `'\0'`.
///
/// # Safety
///
/// If the given character is `'\0'`, this will cause an underflow.
/// (Thus, it will panic in debug mode, undefined behavior in release mode.)
pub unsafe fn backward(ch: char) -> char {
    if ch == AFTER_SURROGATE {
        BEFORE_SURROGATE
    } else {
        char::from_u32_unchecked(ch as u32 - 1)
    }
}
