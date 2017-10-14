//! A collection of serialization and deserialization functions
//! that use the `serde` crate for the serializable and deserializable
//! implementation.

use std::io::{self, Write, Read};
use std::{error, fmt, result};
use std::str::Utf8Error;
use ::{CountSize, SizeLimit};
use byteorder::{ByteOrder};
use std::error::Error as StdError;

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
pub type Error = Box<ErrorKind>;

/// The kind of error that can be produced during a serialization or deserialization.
#[derive(Debug)]
pub enum ErrorKind {
    /// If the error stems from the reader/writer that is being used
    /// during (de)serialization, that error will be stored and returned here.
    Io(io::Error),
    /// Returned if the deserializer attempts to deserialize a string that is not valid utf8
    InvalidUtf8Encoding(Utf8Error),
    /// Returned if the deserializer attempts to deserialize a bool that was
    /// not encoded as either a 1 or a 0
    InvalidBoolEncoding(u8),
    /// Returned if the deserializer attempts to deserialize a char that is not in the correct format.
    InvalidCharEncoding,
    /// Returned if the deserializer attempts to deserialize the tag of an enum that is
    /// not in the expected ranges
    InvalidTagEncoding(usize),
    /// Serde has a deserialize_any method that lets the format hint to the
    /// object which route to take in deserializing.
    DeserializeAnyNotSupported,
    /// If (de)serializing a message takes more than the provided size limit, this
    /// error is returned.
    SizeLimit,
    /// Bincode can not encode sequences of unknown length (like iterators).
    SequenceMustHaveLength,
    /// A custom error message from Serde.
    Custom(String)
}

impl StdError for ErrorKind {
    fn description(&self) -> &str {
        match *self {
            ErrorKind::Io(ref err) => error::Error::description(err),
            ErrorKind::InvalidUtf8Encoding(_) => "string is not valid utf8",
            ErrorKind::InvalidBoolEncoding(_) => "invalid u8 while decoding bool",
            ErrorKind::InvalidCharEncoding => "char is not valid",
            ErrorKind::InvalidTagEncoding(_) => "tag for enum is not valid",
            ErrorKind::SequenceMustHaveLength => "bincode can't encode infinite sequences",
            ErrorKind::DeserializeAnyNotSupported => "bincode doesn't support serde::Deserializer::deserialize_any",
            ErrorKind::SizeLimit => "the size limit for decoding has been reached",
            ErrorKind::Custom(ref msg) => msg,

        }
    }

    fn cause(&self) -> Option<&error::Error> {
        match *self {
            ErrorKind::Io(ref err) => Some(err),
            ErrorKind::InvalidUtf8Encoding(_) => None,
            ErrorKind::InvalidBoolEncoding(_) => None,
            ErrorKind::InvalidCharEncoding => None,
            ErrorKind::InvalidTagEncoding(_) => None,
            ErrorKind::SequenceMustHaveLength => None,
            ErrorKind::DeserializeAnyNotSupported => None,
            ErrorKind::SizeLimit => None,
            ErrorKind::Custom(_) => None,
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Error {
        ErrorKind::Io(err).into()
    }
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ErrorKind::Io(ref ioerr) =>
                write!(fmt, "io error: {}", ioerr),
            ErrorKind::InvalidUtf8Encoding(ref e) =>
                write!(fmt, "{}: {}", self.description(), e),
            ErrorKind::InvalidBoolEncoding(b) =>
                write!(fmt, "{}, expected 0 or 1, found {}", self.description(), b),
            ErrorKind::InvalidCharEncoding =>
                write!(fmt, "{}", self.description()),
            ErrorKind::InvalidTagEncoding(tag) =>
                write!(fmt, "{}, found {}", self.description(), tag),
            ErrorKind::SequenceMustHaveLength =>
                write!(fmt, "bincode can only encode sequences and maps that have a knowable size ahead of time."),
            ErrorKind::SizeLimit =>
                write!(fmt, "size limit was exceeded"),
            ErrorKind::DeserializeAnyNotSupported=>
                write!(fmt, "bincode does not support the serde::Deserializer::deserialize_any method"),
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
pub fn serialize_into<W, T: ?Sized, S, E>(writer: W, value: &T, size_limit: S) -> Result<()>
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

/// Deserializes an object directly from a `Read`er.
///
/// If the provided `SizeLimit` is reached, the deserialization will bail immediately.
/// A SizeLimit can help prevent an attacker from flooding your server with
/// a neverending stream of values that runs your server out of memory.
///
/// If this returns an `Error`, assume that the buffer that you passed
/// in is in an invalid state, as the error could be returned during any point
/// in the reading.
pub fn deserialize_from<R, T, S, E>(reader: R, size_limit: S) -> Result<T>
    where R: Read, T: serde::de::DeserializeOwned, S: SizeLimit, E: ByteOrder
{
    let reader = ::de::read::IoReader::new(reader);
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
