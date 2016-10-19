// Copyright 2013-2014 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.


//! Abstraction that conditionally compiles either to rust-encoding,
//! or to only support UTF-8.

#[cfg(feature = "query_encoding")] extern crate encoding;

use std::borrow::Cow;

#[cfg(feature = "query_encoding")] use self::encoding::types::{DecoderTrap, EncoderTrap};
#[cfg(feature = "query_encoding")] use self::encoding::label::encoding_from_whatwg_label;
#[cfg(feature = "query_encoding")] pub use self::encoding::types::EncodingRef;

#[cfg(feature = "query_encoding")]
#[derive(Copy, Clone)]
pub struct EncodingOverride {
    /// `None` means UTF-8.
    encoding: Option<EncodingRef>
}

#[cfg(feature = "query_encoding")]
impl EncodingOverride {
    pub fn from_opt_encoding(encoding: Option<EncodingRef>) -> Self {
        encoding.map(Self::from_encoding).unwrap_or_else(Self::utf8)
    }

    pub fn from_encoding(encoding: EncodingRef) -> Self {
        EncodingOverride {
            encoding: if encoding.name() == "utf-8" { None } else { Some(encoding) }
        }
    }

    #[inline]
    pub fn utf8() -> Self {
        EncodingOverride { encoding: None }
    }

    pub fn lookup(label: &[u8]) -> Option<Self> {
        // Don't use String::from_utf8_lossy since no encoding label contains U+FFFD
        // https://encoding.spec.whatwg.org/#names-and-labels
        ::std::str::from_utf8(label)
        .ok()
        .and_then(encoding_from_whatwg_label)
        .map(Self::from_encoding)
    }

    /// https://encoding.spec.whatwg.org/#get-an-output-encoding
    pub fn to_output_encoding(self) -> Self {
        if let Some(encoding) = self.encoding {
            if matches!(encoding.name(), "utf-16le" | "utf-16be") {
                return Self::utf8()
            }
        }
        self
    }

    pub fn is_utf8(&self) -> bool {
        self.encoding.is_none()
    }

    pub fn name(&self) -> &'static str {
        match self.encoding {
            Some(encoding) => encoding.name(),
            None => "utf-8",
        }
    }

    pub fn decode<'a>(&self, input: Cow<'a, [u8]>) -> Cow<'a, str> {
        match self.encoding {
            // `encoding.decode` never returns `Err` when called with `DecoderTrap::Replace`
            Some(encoding) => encoding.decode(&input, DecoderTrap::Replace).unwrap().into(),
            None => decode_utf8_lossy(input),
        }
    }

    pub fn encode<'a>(&self, input: Cow<'a, str>) -> Cow<'a, [u8]> {
        match self.encoding {
            // `encoding.encode` never returns `Err` when called with `EncoderTrap::NcrEscape`
            Some(encoding) => Cow::Owned(encoding.encode(&input, EncoderTrap::NcrEscape).unwrap()),
            None => encode_utf8(input)
        }
    }
}


#[cfg(not(feature = "query_encoding"))]
#[derive(Copy, Clone)]
pub struct EncodingOverride;

#[cfg(not(feature = "query_encoding"))]
impl EncodingOverride {
    #[inline]
    pub fn utf8() -> Self {
        EncodingOverride
    }

    pub fn decode<'a>(&self, input: Cow<'a, [u8]>) -> Cow<'a, str> {
        decode_utf8_lossy(input)
    }

    pub fn encode<'a>(&self, input: Cow<'a, str>) -> Cow<'a, [u8]> {
        encode_utf8(input)
    }
}

pub fn decode_utf8_lossy(input: Cow<[u8]>) -> Cow<str> {
    match input {
        Cow::Borrowed(bytes) => String::from_utf8_lossy(bytes),
        Cow::Owned(bytes) => {
            let raw_utf8: *const [u8];
            match String::from_utf8_lossy(&bytes) {
                Cow::Borrowed(utf8) => raw_utf8 = utf8.as_bytes(),
                Cow::Owned(s) => return s.into(),
            }
            // from_utf8_lossy returned a borrow of `bytes` unchanged.
            debug_assert!(raw_utf8 == &*bytes as *const [u8]);
            // Reuse the existing `Vec` allocation.
            unsafe { String::from_utf8_unchecked(bytes) }.into()
        }
    }
}

pub fn encode_utf8(input: Cow<str>) -> Cow<[u8]> {
    match input {
        Cow::Borrowed(s) => Cow::Borrowed(s.as_bytes()),
        Cow::Owned(s) => Cow::Owned(s.into_bytes())
    }
}
