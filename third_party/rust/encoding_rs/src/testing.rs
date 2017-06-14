// Copyright 2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;

pub fn decode(encoding: &'static Encoding, bytes: &[u8], expect: &str) {
    let mut vec = Vec::with_capacity(bytes.len() + 32);
    let mut string = String::with_capacity(expect.len() + 32);
    for i in 0usize..32usize {
        vec.clear();
        string.clear();
        for j in 0usize..i {
            let c = 0x40u8 + (j as u8);
            vec.push(c);
            string.push(c as char);
        }
        vec.extend_from_slice(bytes);
        string.push_str(expect);
        decode_without_padding(encoding, &vec[..], &string[..]);
    }
}

pub fn decode_without_padding(encoding: &'static Encoding, bytes: &[u8], expect: &str) {
    decode_to_utf8(encoding, bytes, expect);
    decode_to_utf16(encoding, bytes, &utf16_from_utf8(expect)[..]);
    decode_to_string(encoding, bytes, expect);
}

pub fn encode(encoding: &'static Encoding, str: &str, expect: &[u8]) {
    let mut vec = Vec::with_capacity(expect.len() + 32);
    let mut string = String::with_capacity(str.len() + 32);
    for i in 0usize..32usize {
        vec.clear();
        string.clear();
        for j in 0usize..i {
            let c = 0x40u8 + (j as u8);
            vec.push(c);
            string.push(c as char);
        }
        vec.extend_from_slice(expect);
        string.push_str(str);
        encode_without_padding(encoding, &string[..], &vec[..]);
    }
}

pub fn encode_without_padding(encoding: &'static Encoding, string: &str, expect: &[u8]) {
    encode_from_utf8(encoding, string, expect);
    encode_from_utf16(encoding, &utf16_from_utf8(string)[..], expect);
    encode_to_vec(encoding, string, expect);
}

pub fn decode_to_utf16(encoding: &'static Encoding, bytes: &[u8], expect: &[u16]) {
    let mut decoder = encoding.new_decoder();
    let mut dest: Vec<u16> =
        Vec::with_capacity(decoder.max_utf16_buffer_length(bytes.len()).unwrap());
    let capacity = dest.capacity();
    dest.resize(capacity, 0u16);
    let (complete, read, written, _) = decoder.decode_to_utf16(bytes, &mut dest, true);
    match complete {
        CoderResult::InputEmpty => {}
        CoderResult::OutputFull => {
            unreachable!();
        }
    }
    assert_eq!(read, bytes.len());
    assert_eq!(written, expect.len());
    dest.truncate(written);
    assert_eq!(&dest[..], expect);
}

pub fn decode_to_utf8(encoding: &'static Encoding, bytes: &[u8], expect: &str) {
    let mut decoder = encoding.new_decoder();
    let mut dest: Vec<u8> =
        Vec::with_capacity(decoder.max_utf8_buffer_length(bytes.len()).unwrap());
    let capacity = dest.capacity();
    dest.resize(capacity, 0u8);
    let (complete, read, written, _) = decoder.decode_to_utf8(bytes, &mut dest, true);
    match complete {
        CoderResult::InputEmpty => {}
        CoderResult::OutputFull => {
            unreachable!();
        }
    }
    assert_eq!(read, bytes.len());
    assert_eq!(written, expect.len());
    dest.truncate(written);
    assert_eq!(&dest[..], expect.as_bytes());
}

pub fn decode_to_string(encoding: &'static Encoding, bytes: &[u8], expect: &str) {
    let (cow, _, _) = encoding.decode(bytes);
    assert_eq!(&cow[..], expect);
}

pub fn encode_from_utf8(encoding: &'static Encoding, string: &str, expect: &[u8]) {
    let mut encoder = encoding.new_encoder();
    let mut dest: Vec<u8> = Vec::with_capacity(10 * (string.len() + 1)); // 10 is replacement worst case
    let capacity = dest.capacity();
    dest.resize(capacity, 0u8);
    let (complete, read, written, _) = encoder.encode_from_utf8(string, &mut dest, true);
    match complete {
        CoderResult::InputEmpty => {}
        CoderResult::OutputFull => {
            unreachable!();
        }
    }
    assert_eq!(read, string.len());
    assert_eq!(written, expect.len());
    dest.truncate(written);
    assert_eq!(&dest[..], expect);
}

pub fn encode_from_utf16(encoding: &'static Encoding, string: &[u16], expect: &[u8]) {
    let mut encoder = encoding.new_encoder();
    let mut dest: Vec<u8> = Vec::with_capacity(10 * (string.len() + 1)); // 10 is replacement worst case
    let capacity = dest.capacity();
    dest.resize(capacity, 0u8);
    let (complete, read, written, _) = encoder.encode_from_utf16(string, &mut dest, true);
    match complete {
        CoderResult::InputEmpty => {}
        CoderResult::OutputFull => {
            unreachable!();
        }
    }
    assert_eq!(read, string.len());
    // assert_eq!(written, expect.len());
    dest.truncate(written);
    assert_eq!(&dest[..], expect);
}

pub fn encode_to_vec(encoding: &'static Encoding, string: &str, expect: &[u8]) {
    let (cow, _, _) = encoding.encode(string);
    assert_eq!(&cow[..], expect);
}

pub fn utf16_from_utf8(string: &str) -> Vec<u16> {
    let mut decoder = UTF_8.new_decoder_without_bom_handling();
    let mut vec = Vec::with_capacity(decoder.max_utf16_buffer_length(string.len()).unwrap());
    let capacity = vec.capacity();
    vec.resize(capacity, 0);

    let (result, read, written) =
        decoder.decode_to_utf16_without_replacement(string.as_bytes(), &mut vec[..], true);
    match result {
        DecoderResult::InputEmpty => {
            debug_assert_eq!(read, string.len());
            vec.resize(written, 0);
            vec
        }
        DecoderResult::Malformed(_, _) => unreachable!("Malformed"),
        DecoderResult::OutputFull => unreachable!("Output full"),
    }
}
