#![deny(missing_docs)]

//! `bincode` is a crate for encoding and decoding using a tiny binary
//! serialization strategy.
//!
//! There are simple functions for encoding to `Vec<u8>` and decoding from
//! `&[u8]`, but the meat of the library is the `encode_into` and `decode_from`
//! functions which respectively allow encoding into a `std::io::Writer`
//! and decoding from a `std::io::Buffer`.
//!
//! ## Modules
//! Until "default type parameters" lands, we have an extra module called `endian_choice`
//! that duplicates all of the core bincode functionality but with the option to choose
//! which endianness the integers are encoded using.
//!
//! The default endianness is little.
//!
//! ### Using Basic Functions
//!
//! ```rust
//! extern crate bincode;
//! use bincode::{serialize, deserialize, Bounded};
//! fn main() {
//!     // The object that we will serialize.
//!     let target = Some("hello world".to_string());
//!     // The maximum size of the encoded message.
//!     let limit = Bounded(20);
//!
//!     let encoded: Vec<u8>        = serialize(&target, limit).unwrap();
//!     let decoded: Option<String> = deserialize(&encoded[..]).unwrap();
//!     assert_eq!(target, decoded);
//! }
//! ```

#![crate_name = "bincode"]
#![crate_type = "rlib"]
#![crate_type = "dylib"]

extern crate byteorder;
extern crate num_traits;
extern crate serde as serde_crate;

mod ser;
mod de;
pub mod internal;

pub mod read_types {
    //! The types that the deserializer uses for optimizations
    pub use ::de::read::{SliceReader, BincodeRead, IoReadReader};
}

use std::io::{Read, Write};

pub use internal::{ErrorKind, Error, Result, serialized_size, serialized_size_bounded};

/// A Deserializer that uses LittleEndian byteorder
pub type Deserializer<W, S> = internal::Deserializer<W, S, byteorder::LittleEndian>;
/// A Serializer that uses LittleEndian byteorder
pub type Serializer<W> = internal::Serializer<W, byteorder::LittleEndian>;

/// Deserializes a slice of bytes into an object.
///
/// This method does not have a size-limit because if you already have the bytes
/// in memory, then you don't gain anything by having a limiter.
pub fn deserialize<'a, T>(bytes: &'a [u8]) -> internal::Result<T>
    where T: serde_crate::de::Deserialize<'a>,
{
    internal::deserialize::<_, byteorder::LittleEndian>(bytes)
}

/// Deserializes an object directly from a `Buffer`ed Reader.
///
/// If the provided `SizeLimit` is reached, the deserialization will bail immediately.
/// A SizeLimit can help prevent an attacker from flooding your server with
/// a neverending stream of values that runs your server out of memory.
///
/// If this returns an `Error`, assume that the buffer that you passed
/// in is in an invalid state, as the error could be returned during any point
/// in the reading.
pub fn deserialize_from<R: ?Sized, T, S>(reader: &mut R, size_limit: S) -> internal::Result<T>
    where R: Read, T: serde_crate::de::DeserializeOwned, S: SizeLimit
{
    internal::deserialize_from::<_, _, _, byteorder::LittleEndian>(reader, size_limit)
}

/// Serializes an object directly into a `Writer`.
///
/// If the serialization would take more bytes than allowed by `size_limit`, an error
/// is returned and *no bytes* will be written into the `Writer`.
///
/// If this returns an `Error` (other than SizeLimit), assume that the
/// writer is in an invalid state, as writing could bail out in the middle of
/// serializing.
pub fn serialize_into<W: ?Sized, T: ?Sized, S>(writer: &mut W, value: &T, size_limit: S) -> internal::Result<()>
    where W: Write, T: serde_crate::Serialize, S: SizeLimit
{
    internal::serialize_into::<_, _, _, byteorder::LittleEndian>(writer, value, size_limit)
}

/// Serializes a serializable object into a `Vec` of bytes.
///
/// If the serialization would take more bytes than allowed by `size_limit`,
/// an error is returned.
pub fn serialize<T: ?Sized, S>(value: &T, size_limit: S) -> internal::Result<Vec<u8>>
    where T: serde_crate::Serialize, S: SizeLimit
{
    internal::serialize::<_, _, byteorder::LittleEndian>(value, size_limit)
}

/// A limit on the amount of bytes that can be read or written.
///
/// Size limits are an incredibly important part of both encoding and decoding.
///
/// In order to prevent DOS attacks on a decoder, it is important to limit the
/// amount of bytes that a single encoded message can be; otherwise, if you
/// are decoding bytes right off of a TCP stream for example, it would be
/// possible for an attacker to flood your server with a 3TB vec, causing the
/// decoder to run out of memory and crash your application!
/// Because of this, you can provide a maximum-number-of-bytes that can be read
/// during decoding, and the decoder will explicitly fail if it has to read
/// any more than that.
///
/// On the other side, you want to make sure that you aren't encoding a message
/// that is larger than your decoder expects.  By supplying a size limit to an
/// encoding function, the encoder will verify that the structure can be encoded
/// within that limit.  This verification occurs before any bytes are written to
/// the Writer, so recovering from an error is easy.
pub trait SizeLimit {
    /// Tells the SizeLimit that a certain number of bytes has been
    /// read or written.  Returns Err if the limit has been exceeded.
    fn add(&mut self, n: u64) -> Result<()>;
    /// Returns the hard limit (if one exists)
    fn limit(&self) -> Option<u64>;
}

/// A SizeLimit that restricts serialized or deserialized messages from
/// exceeding a certain byte length.
#[derive(Copy, Clone)]
pub struct Bounded(pub u64);

/// A SizeLimit without a limit!
/// Use this if you don't care about the size of encoded or decoded messages.
#[derive(Copy, Clone)]
pub struct Infinite;

impl SizeLimit for Bounded {
    #[inline(always)]
    fn add(&mut self, n: u64) -> Result<()> {
        if self.0 >= n {
            self.0 -= n;
            Ok(())
        } else {
            Err(Box::new(ErrorKind::SizeLimit))
        }
    }

    #[inline(always)]
    fn limit(&self) -> Option<u64> { Some(self.0) }
}

impl SizeLimit for Infinite {
    #[inline(always)]
    fn add(&mut self, _: u64) -> Result<()> { Ok (()) }

    #[inline(always)]
    fn limit(&self) -> Option<u64> { None }
}
