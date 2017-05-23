// Copyright 2012-2016 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// Original authors: alexchrichton

//! Extra functions for `char`

// UTF-8 ranges and tags for encoding characters
const TAG_CONT: u8    = 0b1000_0000;
const TAG_TWO_B: u8   = 0b1100_0000;
const TAG_THREE_B: u8 = 0b1110_0000;
const TAG_FOUR_B: u8  = 0b1111_0000;
const MAX_ONE_B: u32   =     0x80;
const MAX_TWO_B: u32   =    0x800;
const MAX_THREE_B: u32 =  0x10000;

/// Placeholder
#[derive(Debug, Copy, Clone)]
pub struct EncodeUtf8Error(());

/// Encode a char into buf using UTF-8.
///
/// On success, return the byte length of the encoding (1, 2, 3 or 4).<br>
/// On error, return `EncodeUtf8Error` if the buffer was too short for the char.
#[inline]
pub fn encode_utf8(ch: char, buf: &mut [u8]) -> Result<usize, EncodeUtf8Error>
{
    let code = ch as u32;
    if code < MAX_ONE_B && buf.len() >= 1 {
        buf[0] = code as u8;
        return Ok(1);
    } else if code < MAX_TWO_B && buf.len() >= 2 {
        buf[0] = (code >> 6 & 0x1F) as u8 | TAG_TWO_B;
        buf[1] = (code & 0x3F) as u8 | TAG_CONT;
        return Ok(2);
    } else if code < MAX_THREE_B && buf.len() >= 3 {
        buf[0] = (code >> 12 & 0x0F) as u8 | TAG_THREE_B;
        buf[1] = (code >>  6 & 0x3F) as u8 | TAG_CONT;
        buf[2] = (code & 0x3F) as u8 | TAG_CONT;
        return Ok(3);
    } else if buf.len() >= 4 {
        buf[0] = (code >> 18 & 0x07) as u8 | TAG_FOUR_B;
        buf[1] = (code >> 12 & 0x3F) as u8 | TAG_CONT;
        buf[2] = (code >>  6 & 0x3F) as u8 | TAG_CONT;
        buf[3] = (code & 0x3F) as u8 | TAG_CONT;
        return Ok(4);
    };
    Err(EncodeUtf8Error(()))
}

