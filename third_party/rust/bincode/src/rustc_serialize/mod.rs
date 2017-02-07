//! A collection of serialization and deserialization functions
//! that use the `rustc_serialize` crate for the encodable and decodable
//! implementation.

use rustc_serialize_crate::{Encodable, Decodable};
use std::io::{Write, Read};
use ::SizeLimit;

pub use self::writer::{SizeChecker, EncoderWriter, EncodingResult, EncodingError};
pub use self::reader::{DecoderReader, DecodingResult, DecodingError, InvalidEncoding};

mod reader;
mod writer;

/// Encodes an encodable object into a `Vec` of bytes.
///
/// If the encoding would take more bytes than allowed by `size_limit`,
/// an error is returned.
pub fn encode<T: Encodable>(t: &T, size_limit: SizeLimit) -> EncodingResult<Vec<u8>> {
    // Since we are putting values directly into a vector, we can do size
    // computation out here and pre-allocate a buffer of *exactly*
    // the right size.
    let mut w = if let SizeLimit::Bounded(l) = size_limit {
        let actual_size = encoded_size_bounded(t, l);
        let actual_size = try!(actual_size.ok_or(EncodingError::SizeLimit));
        Vec::with_capacity(actual_size as usize)
    } else {
        vec![]
    };

    match encode_into(t, &mut w, SizeLimit::Infinite) {
        Ok(()) => Ok(w),
        Err(e) => Err(e)
    }
}

/// Decodes a slice of bytes into an object.
///
/// This method does not have a size-limit because if you already have the bytes
/// in memory, then you don't gain anything by having a limiter.
pub fn decode<T: Decodable>(b: &[u8]) -> DecodingResult<T> {
    let mut b = b;
    decode_from(&mut b, SizeLimit::Infinite)
}

/// Encodes an object directly into a `Writer`.
///
/// If the encoding would take more bytes than allowed by `size_limit`, an error
/// is returned and *no bytes* will be written into the `Writer`.
///
/// If this returns an `EncodingError` (other than SizeLimit), assume that the
/// writer is in an invalid state, as writing could bail out in the middle of
/// encoding.
pub fn encode_into<T: Encodable, W: Write>(t: &T,
                                           w: &mut W,
                                           size_limit: SizeLimit)
                                           -> EncodingResult<()> {
    try!(match size_limit {
        SizeLimit::Infinite => Ok(()),
        SizeLimit::Bounded(x) => {
            let mut size_checker = SizeChecker::new(x);
            t.encode(&mut size_checker)
        }
    });

    t.encode(&mut writer::EncoderWriter::new(w))
}

/// Decoes an object directly from a `Buffer`ed Reader.
///
/// If the provided `SizeLimit` is reached, the decode will bail immediately.
/// A SizeLimit can help prevent an attacker from flooding your server with
/// a neverending stream of values that runs your server out of memory.
///
/// If this returns an `DecodingError`, assume that the buffer that you passed
/// in is in an invalid state, as the error could be returned during any point
/// in the reading.
pub fn decode_from<R: Read, T: Decodable>(r: &mut R, size_limit: SizeLimit) -> DecodingResult<T> {
    Decodable::decode(&mut reader::DecoderReader::new(r, size_limit))
}


/// Returns the size that an object would be if encoded using bincode.
///
/// This is used internally as part of the check for encode_into, but it can
/// be useful for preallocating buffers if thats your style.
pub fn encoded_size<T: Encodable>(t: &T) -> u64 {
    use std::u64::MAX;
    let mut size_checker = SizeChecker::new(MAX);
    t.encode(&mut size_checker).ok();
    size_checker.written
}

/// Given a maximum size limit, check how large an object would be if it
/// were to be encoded.
///
/// If it can be encoded in `max` or fewer bytes, that number will be returned
/// inside `Some`.  If it goes over bounds, then None is returned.
pub fn encoded_size_bounded<T: Encodable>(t: &T, max: u64) -> Option<u64> {
    let mut size_checker = SizeChecker::new(max);
    t.encode(&mut size_checker).ok().map(|_| size_checker.written)
}
