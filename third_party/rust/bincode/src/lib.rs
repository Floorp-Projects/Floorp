#![deny(missing_docs)]

//! Bincode is a crate for encoding and decoding using a tiny binary
//! serialization strategy.  Using it, you can easily go from having
//! an object in memory, quickly serialize it to bytes, and then
//! deserialize it back just as fast!
//!
//! ### Using Basic Functions
//!
//! ```rust
//! extern crate bincode;
//! use bincode::{serialize, deserialize};
//! fn main() {
//!     // The object that we will serialize.
//!     let target: Option<String>  = Some("hello world".to_string());
//!
//!     let encoded: Vec<u8>        = serialize(&target).unwrap();
//!     let decoded: Option<String> = deserialize(&encoded[..]).unwrap();
//!     assert_eq!(target, decoded);
//! }
//! ```

#![crate_name = "bincode"]
#![crate_type = "rlib"]
#![crate_type = "dylib"]

extern crate byteorder;
extern crate serde;

mod config;
mod ser;
mod error;
mod de;
mod internal;

pub use error::{Error, ErrorKind, Result};
pub use config::Config;
pub use de::read::{BincodeRead, IoReader, SliceReader};

/// An object that implements this trait can be passed a
/// serde::Deserializer without knowing its concrete type.
///
/// This trait should be used only for `with_deserializer` functions.
#[doc(hidden)]
pub trait DeserializerAcceptor<'a> {
    /// The return type for the accept method
    type Output;
    /// Accept a serde::Deserializer and do whatever you want with it.
    fn accept<T: serde::Deserializer<'a>>(self, T) -> Self::Output;
}

/// An object that implements this trait can be passed a
/// serde::Serializer without knowing its concrete type.
///
/// This trait should be used only for `with_serializer` functions.
#[doc(hidden)]
pub trait SerializerAcceptor {
    /// The return type for the accept method
    type Output;
    /// Accept a serde::Serializer and do whatever you want with it.
    fn accept<T: serde::Serializer>(self, T) -> Self::Output;
}

/// Get a default configuration object.
///
/// ### Default Configuration:
///
/// | Byte limit | Endianness |
/// |------------|------------|
/// | Unlimited  | Little     |
pub fn config() -> Config {
    Config::new()
}

/// Serializes an object directly into a `Writer` using the default configuration.
///
/// If the serialization would take more bytes than allowed by the size limit, an error
/// is returned and *no bytes* will be written into the `Writer`.
pub fn serialize_into<W, T: ?Sized>(writer: W, value: &T) -> Result<()>
where
    W: std::io::Write,
    T: serde::Serialize,
{
    config().serialize_into(writer, value)
}

/// Serializes a serializable object into a `Vec` of bytes using the default configuration.
pub fn serialize<T: ?Sized>(value: &T) -> Result<Vec<u8>>
where
    T: serde::Serialize,
{
    config().serialize(value)
}

/// Deserializes an object directly from a `Read`er using the default configuration.
///
/// If this returns an `Error`, `reader` may be in an invalid state.
pub fn deserialize_from<R, T>(reader: R) -> Result<T>
where
    R: std::io::Read,
    T: serde::de::DeserializeOwned,
{
    config().deserialize_from(reader)
}

/// Deserializes an object from a custom `BincodeRead`er using the default configuration.
/// It is highly recommended to use `deserialize_from` unless you need to implement
/// `BincodeRead` for performance reasons.
///
/// If this returns an `Error`, `reader` may be in an invalid state.
pub fn deserialize_from_custom<'a, R, T>(reader: R) -> Result<T>
where
    R: de::read::BincodeRead<'a>,
    T: serde::de::DeserializeOwned,
{
    config().deserialize_from_custom(reader)
}

/// Only use this if you know what you're doing.
///
/// This is part of the public API.
#[doc(hidden)]
pub fn deserialize_in_place<'a, R, T>(reader: R, place: &mut T) -> Result<()>
where
    T: serde::de::Deserialize<'a>,
    R: BincodeRead<'a>
{
    config().deserialize_in_place(reader, place)
}

/// Deserializes a slice of bytes into an instance of `T` using the default configuration.
pub fn deserialize<'a, T>(bytes: &'a [u8]) -> Result<T>
where
    T: serde::de::Deserialize<'a>,
{
    config().deserialize(bytes)
}

/// Returns the size that an object would be if serialized using Bincode with the default configuration.
pub fn serialized_size<T: ?Sized>(value: &T) -> Result<u64>
where
    T: serde::Serialize,
{
    config().serialized_size(value)
}

/// Executes the acceptor with a serde::Deserializer instance.
/// NOT A PART OF THE STABLE PUBLIC API
#[doc(hidden)]
pub fn with_deserializer<'a, A,  R>(reader: R, acceptor: A) -> A::Output
where A: DeserializerAcceptor<'a>,
        R: BincodeRead<'a>
{
    config().with_deserializer(reader, acceptor)
}

/// Executes the acceptor with a serde::Serializer instance.
/// NOT A PART OF THE STABLE PUBLIC API
#[doc(hidden)]
pub fn with_serializer<A, W>(writer: W, acceptor: A) -> A::Output
where A: SerializerAcceptor,
    W: std::io::Write
{
    config().with_serializer(writer, acceptor)
}
