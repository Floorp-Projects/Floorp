// SPDX-License-Identifier: MPL-2.0

//! Module `codec` provides support for encoding and decoding messages to or from the TLS wire
//! encoding, as specified in [RFC 8446, Section 3][1]. It provides traits that can be implemented
//! on values that need to be encoded or decoded, as well as utility functions for encoding
//! sequences of values.
//!
//! [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3

use byteorder::{BigEndian, ReadBytesExt};
use std::{
    error::Error,
    io::{Cursor, Read},
    mem::size_of,
};

#[allow(missing_docs)]
#[derive(Debug, thiserror::Error)]
pub enum CodecError {
    #[error("I/O error")]
    Io(#[from] std::io::Error),
    #[error("{0} bytes left in buffer after decoding value")]
    BytesLeftOver(usize),
    #[error("length prefix of encoded vector overflows buffer: {0}")]
    LengthPrefixTooBig(usize),
    #[error("other error: {0}")]
    Other(#[source] Box<dyn Error + 'static + Send + Sync>),
    #[error("unexpected value")]
    UnexpectedValue,
}

/// Describes how to decode an object from a byte sequence.
pub trait Decode: Sized {
    /// Read and decode an encoded object from `bytes`. On success, the decoded value is returned
    /// and `bytes` is advanced by the encoded size of the value. On failure, an error is returned
    /// and no further attempt to read from `bytes` should be made.
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError>;

    /// Convenience method to get decoded value. Returns an error if [`Self::decode`] fails, or if
    /// there are any bytes left in `bytes` after decoding a value.
    fn get_decoded(bytes: &[u8]) -> Result<Self, CodecError> {
        Self::get_decoded_with_param(&(), bytes)
    }
}

/// Describes how to decode an object from a byte sequence, with a decoding parameter provided to
/// provide additional data used in decoding.
pub trait ParameterizedDecode<P>: Sized {
    /// Read and decode an encoded object from `bytes`. `decoding_parameter` provides details of the
    /// wire encoding such as lengths of different portions of the message. On success, the decoded
    /// value is returned and `bytes` is advanced by the encoded size of the value. On failure, an
    /// error is returned and no further attempt to read from `bytes` should be made.
    fn decode_with_param(
        decoding_parameter: &P,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError>;

    /// Convenience method to get decoded value. Returns an error if [`Self::decode_with_param`]
    /// fails, or if there are any bytes left in `bytes` after decoding a value.
    fn get_decoded_with_param(decoding_parameter: &P, bytes: &[u8]) -> Result<Self, CodecError> {
        let mut cursor = Cursor::new(bytes);
        let decoded = Self::decode_with_param(decoding_parameter, &mut cursor)?;
        if cursor.position() as usize != bytes.len() {
            return Err(CodecError::BytesLeftOver(
                bytes.len() - cursor.position() as usize,
            ));
        }

        Ok(decoded)
    }
}

// Provide a blanket implementation so that any Decode can be used as a ParameterizedDecode<T> for
// any T.
impl<D: Decode + ?Sized, T> ParameterizedDecode<T> for D {
    fn decode_with_param(
        _decoding_parameter: &T,
        bytes: &mut Cursor<&[u8]>,
    ) -> Result<Self, CodecError> {
        Self::decode(bytes)
    }
}

/// Describes how to encode objects into a byte sequence.
pub trait Encode {
    /// Append the encoded form of this object to the end of `bytes`, growing the vector as needed.
    fn encode(&self, bytes: &mut Vec<u8>);

    /// Convenience method to get encoded value.
    fn get_encoded(&self) -> Vec<u8> {
        self.get_encoded_with_param(&())
    }
}

/// Describes how to encode objects into a byte sequence.
pub trait ParameterizedEncode<P> {
    /// Append the encoded form of this object to the end of `bytes`, growing the vector as needed.
    /// `encoding_parameter` provides details of the wire encoding, used to control how the value
    /// is encoded.
    fn encode_with_param(&self, encoding_parameter: &P, bytes: &mut Vec<u8>);

    /// Convenience method to get encoded value.
    fn get_encoded_with_param(&self, encoding_parameter: &P) -> Vec<u8> {
        let mut ret = Vec::new();
        self.encode_with_param(encoding_parameter, &mut ret);
        ret
    }
}

// Provide a blanket implementation so that any Encode can be used as a ParameterizedEncode<T> for
// any T.
impl<E: Encode + ?Sized, T> ParameterizedEncode<T> for E {
    fn encode_with_param(&self, _encoding_parameter: &T, bytes: &mut Vec<u8>) {
        self.encode(bytes)
    }
}

impl Decode for () {
    fn decode(_bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(())
    }
}

impl Encode for () {
    fn encode(&self, _bytes: &mut Vec<u8>) {}
}

impl Decode for u8 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let mut value = [0u8; size_of::<u8>()];
        bytes.read_exact(&mut value)?;
        Ok(value[0])
    }
}

impl Encode for u8 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.push(*self);
    }
}

impl Decode for u16 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(bytes.read_u16::<BigEndian>()?)
    }
}

impl Encode for u16 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&u16::to_be_bytes(*self));
    }
}

/// 24 bit integer, per
/// [RFC 8443, section 3.3](https://datatracker.ietf.org/doc/html/rfc8446#section-3.3)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct U24(pub u32);

impl Decode for U24 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(U24(bytes.read_u24::<BigEndian>()?))
    }
}

impl Encode for U24 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        // Encode lower three bytes of the u32 as u24
        bytes.extend_from_slice(&u32::to_be_bytes(self.0)[1..]);
    }
}

impl Decode for u32 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(bytes.read_u32::<BigEndian>()?)
    }
}

impl Encode for u32 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&u32::to_be_bytes(*self));
    }
}

impl Decode for u64 {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(bytes.read_u64::<BigEndian>()?)
    }
}

impl Encode for u64 {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&u64::to_be_bytes(*self));
    }
}

/// Encode `items` into `bytes` as a [variable-length vector][1] with a maximum length of `0xff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
pub fn encode_u8_items<P, E: ParameterizedEncode<P>>(
    bytes: &mut Vec<u8>,
    encoding_parameter: &P,
    items: &[E],
) {
    // Reserve space to later write length
    let len_offset = bytes.len();
    bytes.push(0);

    for item in items {
        item.encode_with_param(encoding_parameter, bytes);
    }

    let len = bytes.len() - len_offset - 1;
    assert!(len <= u8::MAX.into());
    bytes[len_offset] = len as u8;
}

/// Decode `bytes` into a vector of `D` values, treating `bytes` as a [variable-length vector][1] of
/// maximum length `0xff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
pub fn decode_u8_items<P, D: ParameterizedDecode<P>>(
    decoding_parameter: &P,
    bytes: &mut Cursor<&[u8]>,
) -> Result<Vec<D>, CodecError> {
    // Read one byte to get length of opaque byte vector
    let length = usize::from(u8::decode(bytes)?);

    decode_items(length, decoding_parameter, bytes)
}

/// Encode `items` into `bytes` as a [variable-length vector][1] with a maximum length of `0xffff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
pub fn encode_u16_items<P, E: ParameterizedEncode<P>>(
    bytes: &mut Vec<u8>,
    encoding_parameter: &P,
    items: &[E],
) {
    // Reserve space to later write length
    let len_offset = bytes.len();
    0u16.encode(bytes);

    for item in items {
        item.encode_with_param(encoding_parameter, bytes);
    }

    let len = bytes.len() - len_offset - 2;
    assert!(len <= u16::MAX.into());
    for (offset, byte) in u16::to_be_bytes(len as u16).iter().enumerate() {
        bytes[len_offset + offset] = *byte;
    }
}

/// Decode `bytes` into a vector of `D` values, treating `bytes` as a [variable-length vector][1] of
/// maximum length `0xffff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
pub fn decode_u16_items<P, D: ParameterizedDecode<P>>(
    decoding_parameter: &P,
    bytes: &mut Cursor<&[u8]>,
) -> Result<Vec<D>, CodecError> {
    // Read two bytes to get length of opaque byte vector
    let length = usize::from(u16::decode(bytes)?);

    decode_items(length, decoding_parameter, bytes)
}

/// Encode `items` into `bytes` as a [variable-length vector][1] with a maximum length of
/// `0xffffff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4.
pub fn encode_u24_items<P, E: ParameterizedEncode<P>>(
    bytes: &mut Vec<u8>,
    encoding_parameter: &P,
    items: &[E],
) {
    // Reserve space to later write length
    let len_offset = bytes.len();
    U24(0).encode(bytes);

    for item in items {
        item.encode_with_param(encoding_parameter, bytes);
    }

    let len = bytes.len() - len_offset - 3;
    assert!(len <= 0xffffff);
    for (offset, byte) in u32::to_be_bytes(len as u32)[1..].iter().enumerate() {
        bytes[len_offset + offset] = *byte;
    }
}

/// Decode `bytes` into a vector of `D` values, treating `bytes` as a [variable-length vector][1] of
/// maximum length `0xffffff`.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8446#section-3.4
pub fn decode_u24_items<P, D: ParameterizedDecode<P>>(
    decoding_parameter: &P,
    bytes: &mut Cursor<&[u8]>,
) -> Result<Vec<D>, CodecError> {
    // Read three bytes to get length of opaque byte vector
    let length = U24::decode(bytes)?.0 as usize;

    decode_items(length, decoding_parameter, bytes)
}

/// Decode the next `length` bytes from `bytes` into as many instances of `D` as possible.
fn decode_items<P, D: ParameterizedDecode<P>>(
    length: usize,
    decoding_parameter: &P,
    bytes: &mut Cursor<&[u8]>,
) -> Result<Vec<D>, CodecError> {
    let mut decoded = Vec::new();
    let initial_position = bytes.position() as usize;

    // Create cursor over specified portion of provided cursor to ensure we can't read past length.
    let inner = bytes.get_ref();

    // Make sure encoded length doesn't overflow usize or go past the end of provided byte buffer.
    let (items_end, overflowed) = initial_position.overflowing_add(length);
    if overflowed || items_end > inner.len() {
        return Err(CodecError::LengthPrefixTooBig(length));
    }

    let mut sub = Cursor::new(&bytes.get_ref()[initial_position..items_end]);

    while sub.position() < length as u64 {
        decoded.push(D::decode_with_param(decoding_parameter, &mut sub)?);
    }

    // Advance outer cursor by the amount read in the inner cursor
    bytes.set_position(initial_position as u64 + sub.position());

    Ok(decoded)
}

#[cfg(test)]
mod tests {

    use super::*;
    use assert_matches::assert_matches;

    #[test]
    fn encode_nothing() {
        let mut bytes = vec![];
        ().encode(&mut bytes);
        assert_eq!(bytes.len(), 0);
    }

    #[test]
    fn roundtrip_u8() {
        let value = 100u8;

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), 1);

        let decoded = u8::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    #[test]
    fn roundtrip_u16() {
        let value = 1000u16;

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), 2);
        // Check endianness of encoding
        assert_eq!(bytes, vec![3, 232]);

        let decoded = u16::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    #[test]
    fn roundtrip_u24() {
        let value = U24(1_000_000u32);

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), 3);
        // Check endianness of encoding
        assert_eq!(bytes, vec![15, 66, 64]);

        let decoded = U24::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    #[test]
    fn roundtrip_u32() {
        let value = 134_217_728u32;

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), 4);
        // Check endianness of encoding
        assert_eq!(bytes, vec![8, 0, 0, 0]);

        let decoded = u32::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    #[test]
    fn roundtrip_u64() {
        let value = 137_438_953_472u64;

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), 8);
        // Check endianness of encoding
        assert_eq!(bytes, vec![0, 0, 0, 32, 0, 0, 0, 0]);

        let decoded = u64::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    #[derive(Debug, Eq, PartialEq)]
    struct TestMessage {
        field_u8: u8,
        field_u16: u16,
        field_u24: U24,
        field_u32: u32,
        field_u64: u64,
    }

    impl Encode for TestMessage {
        fn encode(&self, bytes: &mut Vec<u8>) {
            self.field_u8.encode(bytes);
            self.field_u16.encode(bytes);
            self.field_u24.encode(bytes);
            self.field_u32.encode(bytes);
            self.field_u64.encode(bytes);
        }
    }

    impl Decode for TestMessage {
        fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
            let field_u8 = u8::decode(bytes)?;
            let field_u16 = u16::decode(bytes)?;
            let field_u24 = U24::decode(bytes)?;
            let field_u32 = u32::decode(bytes)?;
            let field_u64 = u64::decode(bytes)?;

            Ok(TestMessage {
                field_u8,
                field_u16,
                field_u24,
                field_u32,
                field_u64,
            })
        }
    }

    impl TestMessage {
        fn encoded_length() -> usize {
            // u8 field
            1 +
            // u16 field
            2 +
            // u24 field
            3 +
            // u32 field
            4 +
            // u64 field
            8
        }
    }

    #[test]
    fn roundtrip_message() {
        let value = TestMessage {
            field_u8: 0,
            field_u16: 300,
            field_u24: U24(1_000_000),
            field_u32: 134_217_728,
            field_u64: 137_438_953_472,
        };

        let mut bytes = vec![];
        value.encode(&mut bytes);
        assert_eq!(bytes.len(), TestMessage::encoded_length());

        let decoded = TestMessage::decode(&mut Cursor::new(&bytes)).unwrap();
        assert_eq!(value, decoded);
    }

    fn messages_vec() -> Vec<TestMessage> {
        vec![
            TestMessage {
                field_u8: 0,
                field_u16: 300,
                field_u24: U24(1_000_000),
                field_u32: 134_217_728,
                field_u64: 137_438_953_472,
            },
            TestMessage {
                field_u8: 0,
                field_u16: 300,
                field_u24: U24(1_000_000),
                field_u32: 134_217_728,
                field_u64: 137_438_953_472,
            },
            TestMessage {
                field_u8: 0,
                field_u16: 300,
                field_u24: U24(1_000_000),
                field_u32: 134_217_728,
                field_u64: 137_438_953_472,
            },
        ]
    }

    #[test]
    fn roundtrip_variable_length_u8() {
        let values = messages_vec();
        let mut bytes = vec![];
        encode_u8_items(&mut bytes, &(), &values);

        assert_eq!(
            bytes.len(),
            // Length of opaque vector
            1 +
            // 3 TestMessage values
            3 * TestMessage::encoded_length()
        );

        let decoded = decode_u8_items(&(), &mut Cursor::new(&bytes)).unwrap();
        assert_eq!(values, decoded);
    }

    #[test]
    fn roundtrip_variable_length_u16() {
        let values = messages_vec();
        let mut bytes = vec![];
        encode_u16_items(&mut bytes, &(), &values);

        assert_eq!(
            bytes.len(),
            // Length of opaque vector
            2 +
            // 3 TestMessage values
            3 * TestMessage::encoded_length()
        );

        // Check endianness of encoded length
        assert_eq!(bytes[0..2], [0, 3 * TestMessage::encoded_length() as u8]);

        let decoded = decode_u16_items(&(), &mut Cursor::new(&bytes)).unwrap();
        assert_eq!(values, decoded);
    }

    #[test]
    fn roundtrip_variable_length_u24() {
        let values = messages_vec();
        let mut bytes = vec![];
        encode_u24_items(&mut bytes, &(), &values);

        assert_eq!(
            bytes.len(),
            // Length of opaque vector
            3 +
            // 3 TestMessage values
            3 * TestMessage::encoded_length()
        );

        // Check endianness of encoded length
        assert_eq!(bytes[0..3], [0, 0, 3 * TestMessage::encoded_length() as u8]);

        let decoded = decode_u24_items(&(), &mut Cursor::new(&bytes)).unwrap();
        assert_eq!(values, decoded);
    }

    #[test]
    fn decode_items_overflow() {
        let encoded = vec![1u8];

        let mut cursor = Cursor::new(encoded.as_slice());
        cursor.set_position(1);

        assert_matches!(
            decode_items::<(), u8>(usize::MAX, &(), &mut cursor).unwrap_err(),
            CodecError::LengthPrefixTooBig(usize::MAX)
        );
    }

    #[test]
    fn decode_items_too_big() {
        let encoded = vec![1u8];

        let mut cursor = Cursor::new(encoded.as_slice());
        cursor.set_position(1);

        assert_matches!(
            decode_items::<(), u8>(2, &(), &mut cursor).unwrap_err(),
            CodecError::LengthPrefixTooBig(2)
        );
    }
}
