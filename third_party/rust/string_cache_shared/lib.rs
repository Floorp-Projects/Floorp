// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// FIXME(rust-lang/rust#18153): generate these from an enum
pub const DYNAMIC_TAG: u8 = 0b_00;
pub const INLINE_TAG: u8 = 0b_01;  // len in upper nybble
pub const STATIC_TAG: u8 = 0b_10;
pub const TAG_MASK: u64 = 0b_11;
pub const ENTRY_ALIGNMENT: usize = 4;  // Multiples have TAG_MASK bits unset, available for tagging.

pub const MAX_INLINE_LEN: usize = 7;

pub const STATIC_SHIFT_BITS: usize = 32;

pub fn pack_static(n: u32) -> u64 {
    (STATIC_TAG as u64) | ((n as u64) << STATIC_SHIFT_BITS)
}
