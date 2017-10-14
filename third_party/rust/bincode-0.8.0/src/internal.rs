//! A collection of serialization and deserialization functions
//! that use the `serde` crate for the serializable and deserializable
//! implementation.

use std::io::{Write, Read};
use std::io::Error as IoError;
use std::{error, fmt, result};
use ::SizeLimit;
use byteorder::{ByteOrder};

pub use super::de::{
    Deserializer,
};

pub use super::ser::{
    Serializer,
};

use super::ser::SizeChecker;

use serde_crate as serde;

/// The result of a serialization or deserialization operation.
pub type Result<T> = result::Result<T, Error>;

/// An error that can be produced during (de)serializing.
///
/// If decoding from a Buffer, assume that the buffer has been left
/// in an invalid state.
pub type Error = Box<ErrorKind>;

/// The kind of error that can be produced during a serialization or deserialization.
#[derive(Debug)]
pub enum ErrorKind {
    /// If the error stems from the reader/writer that is being used
    /// during (de)serialization, that error will be stored and returned here.
    IoError(IoError),
    /// If the bytes in the reader are not decodable because of an invalid
    /// encoding, this error will be returned.  This error is only possible
    /// if a stream is corrupted.  A stream produced from `encode` or `encode_into`
    /// should **never** produce an InvalidEncoding error.
    InvalidEncoding {
        #[allow(missing_docs)]
        desc: &'static str,
        #[allow(missing_docs)]
        detail: Option<String>
    },
    /// If (de)serializing a message takes more than the provided size limit, this
    /// error is returned.
    SizeLimit,
    /// Bincode can not encode sequences of unknown length (like iterators).
    SequenceMustHaveLength,
    /// A custom error message from Serde.
    Custom(String)
}

impl error::Error for ErrorKind {
    fn description(&self) -> &str {
        match *self {
            ErrorKind::IoError(ref err) => error::Error::description(err),
            ErrorKind::InvalidEncoding{desc, ..} => desc,
            ErrorKind::SequenceMustHaveLength => "bincode can't encode infinite sequences",
            ErrorKind::SizeLimit => "the size limit for decoding has been reached",
            ErrorKind::Custom(ref msg) => msg,

        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match *self {
            ErrorKind::IoError(ref err) => err.cause(),
            ErrorKind::InvalidEncoding{..} => None,
            ErrorKind::SequenceMustHaveLength => None,
            ErrorKind::SizeLimit => None,
            ErrorKind::Custom(_) => None,
        }
    }
}

impl From<IoError> for Error {
    fn from(err: IoError) -> Error {
        ErrorKind::IoError(err).into()
    }
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ErrorKind::IoError(ref ioerr) =>
                write!(fmt, "IoError: {}", ioerr),
            ErrorKind::InvalidEncoding{desc, detail: None}=>
                write!(fmt, "InvalidEncoding: {}", desc),
            ErrorKind::InvalidEncoding{desc, detail: Some(ref detail)}=>
                write!(fmt, "InvalidEncoding: {} ({})", desc, detail),
            ErrorKind::SequenceMustHaveLength =>
                write!(fmt, "Bincode can only encode sequences and maps that have a knowable size ahead of time."),
            ErrorKind::SizeLimit =>
                write!(fmt, "SizeLimit"),
            ErrorKind::Custom(ref s) =>
                s.fmt(fmt),
        }
    }
}

impl serde::de::Error for Error {
    fn custom<T: fmt::Display>(desc: T) -> Error {
        ErrorKind::Custom(desc.to_string()).into()
    }
}

impl serde::ser::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        ErrorKind::Custom(msg.to_string()).into()
    }
}

/// Serializes an object directly into a `Writer`.
///
/// If the serialization would take more bytes than allowed by `size_limit`, an error
/// is returned and *no bytes* will be written into the `Writer`.
///
/// If this returns an `Error` (other than SizeLimit), assume that the
/// writer is in an invalid state, as writing could bail out in the middle of
/// serializing.
pub fn serialize_into<W: ?Sized, T: ?Sized, S, E>(writer: &mut W, value: &T, size_limit: S) -> Result<()>
    where W: Write, T: serde::Serialize, S: SizeLimit, E: ByteOrder
{
    if let Some(limit) = size_limit.limit() {
        try!(serialized_size_bounded(value, limit).ok_or(ErrorKind::SizeLimit));
    }

    let mut serializer = Serializer::<_, E>::new(writer);
    serde::Serialize::serialize(value, &mut serializer)
}

/// Serializes a serializable object into a `Vec` of bytes.
///
/// If the serialization would take more bytes than allowed by `size_limit`,
/// an error is returned.
pub fn serialize<T: ?Sized, S, E>(value: &T, size_limit: S) -> Result<Vec<u8>>
    where T: serde::Serialize, S: SizeLimit, E: ByteOrder
{
    let mut writer = match size_limit.limit() {
        Some(size_limit) => {
            let actual_size = try!(serialized_size_bounded(value, size_limit).ok_or(ErrorKind::SizeLimit));
            Vec::with_capacity(actual_size as usize)
        }
        None => {
            let size = serialized_size(value) as usize;
            Vec::with_capacity(size)
        }
    };

    try!(serialize_into::<_, _, _, E>(&mut writer, value, super::Infinite));
    Ok(writer)
}


struct CountSize {
    total: u64,
    limit: Option<u64>,
}

impl SizeLimit for CountSize {
    fn add(&mut self, c: u64) -> Result<()> {
        self.total += c;
        if let Some(limit) = self.limit {
            if self.total > limit {
                return Err(Box::new(ErrorKind::SizeLimit))
            }
        }
        Ok(())
    }

    fn limit(&self) -> Option<u64> {
        unreachable!();
    }
}

/// Returns the size that an object would be if serialized using bincode.
///
/// This is used internally as part of the check for encode_into, but it can
/// be useful for preallocating buffers if thats your style.
pub fn serialized_size<T: ?Sized>(value: &T) -> u64
    where T: serde::Serialize
{
    let mut size_counter = SizeChecker {
        size_limit: CountSize { total: 0, limit: None }
    };

    value.serialize(&mut size_counter).ok();
    size_counter.size_limit.total
}

/// Given a maximum size limit, check how large an object would be if it
/// were to be serialized.
///
/// If it can be serialized in `max` or fewer bytes, that number will be returned
/// inside `Some`.  If it goes over bounds, then None is returned.
pub fn serialized_size_bounded<T: ?Sized>(value: &T, max: u64) -> Option<u64>
    where T: serde::Serialize
{
    let mut size_counter = SizeChecker {
        size_limit: CountSize { total: 0, limit: Some(max) }
    };

    match value.serialize(&mut size_counter) {
        Ok(_) => Some(size_counter.size_limit.total),
        Err(_) => None,
    }
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
pub fn deserialize_from<R: ?Sized, T, S, E>(reader: &mut R, size_limit: S) -> Result<T>
    where R: Read, T: serde::de::DeserializeOwned, S: SizeLimit, E: ByteOrder
{
    let reader = ::de::read::IoReadReader::new(reader);
    let mut deserializer = Deserializer::<_, S, E>::new(reader, size_limit);
    serde::Deserialize::deserialize(&mut deserializer)
}

/// Deserializes a slice of bytes into an object.
///
/// This method does not have a size-limit because if you already have the bytes
/// in memory, then you don't gain anything by having a limiter.
pub fn deserialize<'a, T, E: ByteOrder>(bytes: &'a [u8]) -> Result<T>
    where T: serde::de::Deserialize<'a>,
{
    let reader = ::de::read::SliceReader::new(bytes);
    let mut deserializer = Deserializer::<_, _, E>::new(reader, super::Infinite);
    serde::Deserialize::deserialize(&mut deserializer)
}
