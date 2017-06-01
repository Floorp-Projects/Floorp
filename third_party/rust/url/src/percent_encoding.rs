// Copyright 2013-2016 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use encoding;
use std::ascii::AsciiExt;
use std::borrow::Cow;
use std::fmt;
use std::slice;
use std::str;

/// Represents a set of characters / bytes that should be percent-encoded.
///
/// See [encode sets specification](http://url.spec.whatwg.org/#simple-encode-set).
///
/// Different characters need to be encoded in different parts of an URL.
/// For example, a literal `?` question mark in an URL’s path would indicate
/// the start of the query string.
/// A question mark meant to be part of the path therefore needs to be percent-encoded.
/// In the query string however, a question mark does not have any special meaning
/// and does not need to be percent-encoded.
///
/// A few sets are defined in this module.
/// Use the [`define_encode_set!`](../macro.define_encode_set!.html) macro to define different ones.
pub trait EncodeSet: Clone {
    /// Called with UTF-8 bytes rather than code points.
    /// Should return true for all non-ASCII bytes.
    fn contains(&self, byte: u8) -> bool;
}

/// Define a new struct
/// that implements the [`EncodeSet`](percent_encoding/trait.EncodeSet.html) trait,
/// for use in [`percent_decode()`](percent_encoding/fn.percent_encode.html)
/// and related functions.
///
/// Parameters are characters to include in the set in addition to those of the base set.
/// See [encode sets specification](http://url.spec.whatwg.org/#simple-encode-set).
///
/// Example
/// =======
///
/// ```rust
/// #[macro_use] extern crate url;
/// use url::percent_encoding::{utf8_percent_encode, SIMPLE_ENCODE_SET};
/// define_encode_set! {
///     /// This encode set is used in the URL parser for query strings.
///     pub QUERY_ENCODE_SET = [SIMPLE_ENCODE_SET] | {' ', '"', '#', '<', '>'}
/// }
/// # fn main() {
/// assert_eq!(utf8_percent_encode("foo bar", QUERY_ENCODE_SET).collect::<String>(), "foo%20bar");
/// # }
/// ```
#[macro_export]
macro_rules! define_encode_set {
    ($(#[$attr: meta])* pub $name: ident = [$base_set: expr] | {$($ch: pat),*}) => {
        $(#[$attr])*
        #[derive(Copy, Clone)]
        #[allow(non_camel_case_types)]
        pub struct $name;

        impl $crate::percent_encoding::EncodeSet for $name {
            #[inline]
            fn contains(&self, byte: u8) -> bool {
                match byte as char {
                    $(
                        $ch => true,
                    )*
                    _ => $base_set.contains(byte)
                }
            }
        }
    }
}

/// This encode set is used for the path of cannot-be-a-base URLs.
#[derive(Copy, Clone)]
#[allow(non_camel_case_types)]
pub struct SIMPLE_ENCODE_SET;

impl EncodeSet for SIMPLE_ENCODE_SET {
    #[inline]
    fn contains(&self, byte: u8) -> bool {
        byte < 0x20 || byte > 0x7E
    }
}

define_encode_set! {
    /// This encode set is used in the URL parser for query strings.
    pub QUERY_ENCODE_SET = [SIMPLE_ENCODE_SET] | {' ', '"', '#', '<', '>'}
}

define_encode_set! {
    /// This encode set is used for path components.
    pub DEFAULT_ENCODE_SET = [QUERY_ENCODE_SET] | {'`', '?', '{', '}'}
}

define_encode_set! {
    /// This encode set is used for on '/'-separated path segment
    pub PATH_SEGMENT_ENCODE_SET = [DEFAULT_ENCODE_SET] | {'%', '/'}
}

define_encode_set! {
    /// This encode set is used for username and password.
    pub USERINFO_ENCODE_SET = [DEFAULT_ENCODE_SET] | {
        '/', ':', ';', '=', '@', '[', '\\', ']', '^', '|'
    }
}

/// Return the percent-encoding of the given bytes.
///
/// This is unconditional, unlike `percent_encode()` which uses an encode set.
pub fn percent_encode_byte(byte: u8) -> &'static str {
    let index = usize::from(byte) * 3;
    &"\
        %00%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F\
        %10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F\
        %20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F\
        %30%31%32%33%34%35%36%37%38%39%3A%3B%3C%3D%3E%3F\
        %40%41%42%43%44%45%46%47%48%49%4A%4B%4C%4D%4E%4F\
        %50%51%52%53%54%55%56%57%58%59%5A%5B%5C%5D%5E%5F\
        %60%61%62%63%64%65%66%67%68%69%6A%6B%6C%6D%6E%6F\
        %70%71%72%73%74%75%76%77%78%79%7A%7B%7C%7D%7E%7F\
        %80%81%82%83%84%85%86%87%88%89%8A%8B%8C%8D%8E%8F\
        %90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F\
        %A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF\
        %B0%B1%B2%B3%B4%B5%B6%B7%B8%B9%BA%BB%BC%BD%BE%BF\
        %C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF\
        %D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF\
        %E0%E1%E2%E3%E4%E5%E6%E7%E8%E9%EA%EB%EC%ED%EE%EF\
        %F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF\
    "[index..index + 3]
}

/// Percent-encode the given bytes with the given encode set.
///
/// The encode set define which bytes (in addition to non-ASCII and controls)
/// need to be percent-encoded.
/// The choice of this set depends on context.
/// For example, `?` needs to be encoded in an URL path but not in a query string.
///
/// The return value is an iterator of `&str` slices (so it has a `.collect::<String>()` method)
/// that also implements `Display` and `Into<Cow<str>>`.
/// The latter returns `Cow::Borrowed` when none of the bytes in `input`
/// are in the given encode set.
#[inline]
pub fn percent_encode<E: EncodeSet>(input: &[u8], encode_set: E) -> PercentEncode<E> {
    PercentEncode {
        bytes: input,
        encode_set: encode_set,
    }
}

/// Percent-encode the UTF-8 encoding of the given string.
///
/// See `percent_encode()` for how to use the return value.
#[inline]
pub fn utf8_percent_encode<E: EncodeSet>(input: &str, encode_set: E) -> PercentEncode<E> {
    percent_encode(input.as_bytes(), encode_set)
}

/// The return type of `percent_encode()` and `utf8_percent_encode()`.
#[derive(Clone)]
pub struct PercentEncode<'a, E: EncodeSet> {
    bytes: &'a [u8],
    encode_set: E,
}

impl<'a, E: EncodeSet> Iterator for PercentEncode<'a, E> {
    type Item = &'a str;

    fn next(&mut self) -> Option<&'a str> {
        if let Some((&first_byte, remaining)) = self.bytes.split_first() {
            if self.encode_set.contains(first_byte) {
                self.bytes = remaining;
                Some(percent_encode_byte(first_byte))
            } else {
                assert!(first_byte.is_ascii());
                for (i, &byte) in remaining.iter().enumerate() {
                    if self.encode_set.contains(byte) {
                        // 1 for first_byte + i for previous iterations of this loop
                        let (unchanged_slice, remaining) = self.bytes.split_at(1 + i);
                        self.bytes = remaining;
                        return Some(unsafe { str::from_utf8_unchecked(unchanged_slice) })
                    } else {
                        assert!(byte.is_ascii());
                    }
                }
                let unchanged_slice = self.bytes;
                self.bytes = &[][..];
                Some(unsafe { str::from_utf8_unchecked(unchanged_slice) })
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        if self.bytes.is_empty() {
            (0, Some(0))
        } else {
            (1, Some(self.bytes.len()))
        }
    }
}

impl<'a, E: EncodeSet> fmt::Display for PercentEncode<'a, E> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        for c in (*self).clone() {
            formatter.write_str(c)?
        }
        Ok(())
    }
}

impl<'a, E: EncodeSet> From<PercentEncode<'a, E>> for Cow<'a, str> {
    fn from(mut iter: PercentEncode<'a, E>) -> Self {
        match iter.next() {
            None => "".into(),
            Some(first) => {
                match iter.next() {
                    None => first.into(),
                    Some(second) => {
                        let mut string = first.to_owned();
                        string.push_str(second);
                        string.extend(iter);
                        string.into()
                    }
                }
            }
        }
    }
}

/// Percent-decode the given bytes.
///
/// The return value is an iterator of decoded `u8` bytes
/// that also implements `Into<Cow<u8>>`
/// (which returns `Cow::Borrowed` when `input` contains no percent-encoded sequence)
/// and has `decode_utf8()` and `decode_utf8_lossy()` methods.
#[inline]
pub fn percent_decode(input: &[u8]) -> PercentDecode {
    PercentDecode {
        bytes: input.iter()
    }
}

/// The return type of `percent_decode()`.
#[derive(Clone)]
pub struct PercentDecode<'a> {
    bytes: slice::Iter<'a, u8>,
}

fn after_percent_sign(iter: &mut slice::Iter<u8>) -> Option<u8> {
    let initial_iter = iter.clone();
    let h = iter.next().and_then(|&b| (b as char).to_digit(16));
    let l = iter.next().and_then(|&b| (b as char).to_digit(16));
    if let (Some(h), Some(l)) = (h, l) {
        Some(h as u8 * 0x10 + l as u8)
    } else {
        *iter = initial_iter;
        None
    }
}

impl<'a> Iterator for PercentDecode<'a> {
    type Item = u8;

    fn next(&mut self) -> Option<u8> {
        self.bytes.next().map(|&byte| {
            if byte == b'%' {
                after_percent_sign(&mut self.bytes).unwrap_or(byte)
            } else {
                byte
            }
        })
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let bytes = self.bytes.len();
        (bytes / 3, Some(bytes))
    }
}

impl<'a> From<PercentDecode<'a>> for Cow<'a, [u8]> {
    fn from(iter: PercentDecode<'a>) -> Self {
        match iter.if_any() {
            Some(vec) => Cow::Owned(vec),
            None => Cow::Borrowed(iter.bytes.as_slice()),
        }
    }
}

impl<'a> PercentDecode<'a> {
    /// If the percent-decoding is different from the input, return it as a new bytes vector.
    pub fn if_any(&self) -> Option<Vec<u8>> {
        let mut bytes_iter = self.bytes.clone();
        while bytes_iter.any(|&b| b == b'%') {
            if let Some(decoded_byte) = after_percent_sign(&mut bytes_iter) {
                let initial_bytes = self.bytes.as_slice();
                let unchanged_bytes_len = initial_bytes.len() - bytes_iter.len() - 3;
                let mut decoded = initial_bytes[..unchanged_bytes_len].to_owned();
                decoded.push(decoded_byte);
                decoded.extend(PercentDecode {
                    bytes: bytes_iter
                });
                return Some(decoded)
            }
        }
        // Nothing to decode
        None
    }

    /// Decode the result of percent-decoding as UTF-8.
    ///
    /// This is return `Err` when the percent-decoded bytes are not well-formed in UTF-8.
    pub fn decode_utf8(self) -> Result<Cow<'a, str>, str::Utf8Error> {
        match self.clone().into() {
            Cow::Borrowed(bytes) => {
                match str::from_utf8(bytes) {
                    Ok(s) => Ok(s.into()),
                    Err(e) => Err(e),
                }
            }
            Cow::Owned(bytes) => {
                match String::from_utf8(bytes) {
                    Ok(s) => Ok(s.into()),
                    Err(e) => Err(e.utf8_error()),
                }
            }
        }
    }

    /// Decode the result of percent-decoding as UTF-8, lossily.
    ///
    /// Invalid UTF-8 percent-encoded byte sequences will be replaced � U+FFFD,
    /// the replacement character.
    pub fn decode_utf8_lossy(self) -> Cow<'a, str> {
        encoding::decode_utf8_lossy(self.clone().into())
    }
}
