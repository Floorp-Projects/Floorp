//! Low-level encoders.
//!
//! This module provides low-level encoders that can be used (for example) to
//! define your own custom section encodings.

use std::convert::TryFrom;

/// Encode a `u32` as a ULEB128.
pub fn u32(n: u32) -> impl ExactSizeIterator<Item = u8> {
    let mut buf = [0; 5];
    let n = leb128::write::unsigned(&mut &mut buf[..], n.into()).unwrap();
    <_>::into_iter(buf).take(n)
}

/// Encode a `u64` as a ULEB128.
pub fn u64(n: u64) -> impl ExactSizeIterator<Item = u8> {
    let mut buf = [0; 10];
    let n = leb128::write::unsigned(&mut &mut buf[..], n.into()).unwrap();
    <_>::into_iter(buf).take(n)
}

/// Encode an `i32` as a SLEB128.
pub fn s32(x: i32) -> impl ExactSizeIterator<Item = u8> {
    let mut buf = [0; 5];
    let n = leb128::write::signed(&mut &mut buf[..], x.into()).unwrap();
    <_>::into_iter(buf).take(n)
}

/// Encode an `i64` that uses at most 33 bits as a SLEB128.
///
/// # Panics
///
/// Panics if more than 33 bits are used.
///
/// ```
/// wasm_encoder::encoders::s33(1 << 32);
/// ```
///
/// ```should_panic
/// wasm_encoder::encoders::s33(1 << 33);
/// ```
///
/// ```
/// wasm_encoder::encoders::s33(-1 << 32);
/// ```
///
/// ```should_panic
/// wasm_encoder::encoders::s33(-1 << 33);
/// ```
pub fn s33(x: i64) -> impl ExactSizeIterator<Item = u8> {
    assert!({
        let mask = 1 << 33 << 30 >> 30;
        x != mask && (x & mask == 0) == (x >= 0)
    });
    let mut buf = [0; 5];
    let n = leb128::write::signed(&mut &mut buf[..], x).unwrap();
    <_>::into_iter(buf).take(n)
}

/// Encode an `i64` as a SLEB128.
pub fn s64(x: i64) -> impl ExactSizeIterator<Item = u8> {
    let mut buf = [0; 10];
    let n = leb128::write::signed(&mut &mut buf[..], x).unwrap();
    <_>::into_iter(buf).take(n)
}

/// Encode a length-prefixed UTF-8 string.
pub fn str<'a>(s: &'a str) -> impl Iterator<Item = u8> + 'a {
    u32(u32::try_from(s.len()).unwrap()).chain(s.as_bytes().iter().copied())
}
