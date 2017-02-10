// This is a part of rust-encoding.
// Copyright (c) 2013-2015, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! Asymmetric or special encoding constructions required by the WHATWG Encoding standard.

use codec;
use types::*;

/// Replacement encoding used to solve a particular attack vector due to mismatching server and
/// client supports for encodings. It is rarely useful outside.
#[derive(Clone, Copy)]
pub struct EncoderOnlyUTF8Encoding;

impl Encoding for EncoderOnlyUTF8Encoding {
    fn name(&self) -> &'static str { "encoder-only-utf-8" }
    fn whatwg_name(&self) -> Option<&'static str> { Some("replacement") } // WHATWG compatibility
    fn raw_encoder(&self) -> Box<RawEncoder> { codec::utf_8::UTF8Encoding.raw_encoder() }
    fn raw_decoder(&self) -> Box<RawDecoder> { codec::error::ErrorEncoding.raw_decoder() }
}

/// Algorithmic mapping for `x-user-defined` encoding.
pub mod x_user_defined {
    #[inline]
    pub fn forward(code: u8) -> u16 {
        0xf700 | (code as u16)
    }

    #[inline]
    pub fn backward(code: u32) -> u8 {
        if (code & !0x7f) == 0xf780 {(code & 0xff) as u8} else {0}
    }
}

