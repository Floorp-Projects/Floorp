// Copyright 2019 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::borrow::Cow;

pub type EncodingOverride<'a> = Option<&'a dyn Fn(&str) -> Cow<[u8]>>;

pub(crate) fn encode<'a>(encoding_override: EncodingOverride, input: &'a str) -> Cow<'a, [u8]> {
    if let Some(o) = encoding_override {
        return o(input);
    }
    input.as_bytes().into()
}

pub(crate) fn decode_utf8_lossy(input: Cow<[u8]>) -> Cow<str> {
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
