//! Wrapper types to enable optimized handling of `&[u8]` and `Vec<u8>`.
//!
//! Without specialization, Rust forces Serde to treat `&[u8]` just like any
//! other slice and `Vec<u8>` just like any other vector. In reality this
//! particular slice and vector can often be serialized and deserialized in a
//! more efficient, compact representation in many formats.
//!
//! When working with such a format, you can opt into specialized handling of
//! `&[u8]` by wrapping it in `serde_bytes::Bytes` and `Vec<u8>` by wrapping it
//! in `serde_bytes::ByteBuf`.
//!
//! This crate supports the Serde `with` attribute to enable efficient handling
//! of `&[u8]` and `Vec<u8>` in structs without needing a wrapper type.
//!
//! ```rust
//! #[macro_use]
//! extern crate serde_derive;
//!
//! extern crate serde;
//! extern crate serde_bytes;
//!
//! #[derive(Serialize)]
//! struct Efficient<'a> {
//!     #[serde(with = "serde_bytes")]
//!     bytes: &'a [u8],
//!
//!     #[serde(with = "serde_bytes")]
//!     byte_buf: Vec<u8>,
//! }
//!
//! #[derive(Serialize, Deserialize)]
//! struct Packet {
//!     #[serde(with = "serde_bytes")]
//!     payload: Vec<u8>,
//! }
//! #
//! # fn main() {}
//! ```

#![doc(html_root_url = "https://docs.rs/serde_bytes/0.10.4")]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "alloc", feature(alloc))]
#![deny(missing_docs)]

#[cfg(feature = "std")]
use std::{fmt, ops};

#[cfg(not(feature = "std"))]
use core::{fmt, ops};

use self::fmt::Debug;

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "alloc")]
use alloc::Vec;

#[macro_use]
extern crate serde;
use serde::ser::{Serialize, Serializer};
use serde::de::{Deserialize, Deserializer, Visitor, Error};

#[cfg(any(feature = "std", feature = "alloc"))]
pub use self::bytebuf::ByteBuf;

mod value;

//////////////////////////////////////////////////////////////////////////////

/// Serde `serialize_with` function to serialize bytes efficiently.
///
/// This function can be used with either of the following Serde attributes:
///
/// - `#[serde(with = "serde_bytes")]`
/// - `#[serde(serialize_with = "serde_bytes::serialize")]`
///
/// ```rust
/// #[macro_use]
/// extern crate serde_derive;
///
/// extern crate serde;
/// extern crate serde_bytes;
///
/// #[derive(Serialize)]
/// struct Efficient<'a> {
///     #[serde(with = "serde_bytes")]
///     bytes: &'a [u8],
///
///     #[serde(with = "serde_bytes")]
///     byte_buf: Vec<u8>,
/// }
/// #
/// # fn main() {}
/// ```
pub fn serialize<T, S>(bytes: &T, serializer: S) -> Result<S::Ok, S::Error>
    where T: ?Sized + AsRef<[u8]>,
          S: Serializer
{
    serializer.serialize_bytes(bytes.as_ref())
}

/// Serde `deserialize_with` function to deserialize bytes efficiently.
///
/// This function can be used with either of the following Serde attributes:
///
/// - `#[serde(with = "serde_bytes")]`
/// - `#[serde(deserialize_with = "serde_bytes::deserialize")]`
///
/// ```rust
/// #[macro_use]
/// extern crate serde_derive;
///
/// extern crate serde;
/// extern crate serde_bytes;
///
/// #[derive(Deserialize)]
/// struct Packet {
///     #[serde(with = "serde_bytes")]
///     payload: Vec<u8>,
/// }
/// #
/// # fn main() {}
/// ```
#[cfg(any(feature = "std", feature = "alloc"))]
pub fn deserialize<'de, T, D>(deserializer: D) -> Result<T, D::Error>
    where T: From<Vec<u8>>,
          D: Deserializer<'de>
{
    ByteBuf::deserialize(deserializer).map(|buf| Into::<Vec<u8>>::into(buf).into())
}

//////////////////////////////////////////////////////////////////////////////

/// Wrapper around `&[u8]` to serialize and deserialize efficiently.
///
/// ```rust
/// extern crate bincode;
/// extern crate serde_bytes;
///
/// use std::collections::HashMap;
/// use std::io;
///
/// use serde_bytes::Bytes;
///
/// fn print_encoded_cache() -> Result<(), bincode::Error> {
///     let mut cache = HashMap::new();
///     cache.insert(3, Bytes::new(b"three"));
///     cache.insert(2, Bytes::new(b"two"));
///     cache.insert(1, Bytes::new(b"one"));
///
///     bincode::serialize_into(&mut io::stdout(), &cache, bincode::Infinite)
/// }
/// #
/// # fn main() {
/// #     print_encoded_cache().unwrap();
/// # }
/// ```
#[derive(Clone, Copy, Eq, Hash, PartialEq, PartialOrd, Ord)]
pub struct Bytes<'a> {
    bytes: &'a [u8],
}

impl<'a> Bytes<'a> {
    /// Wrap an existing `&[u8]`.
    pub fn new(bytes: &'a [u8]) -> Self {
        Bytes { bytes: bytes }
    }
}

impl<'a> Debug for Bytes<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Debug::fmt(self.bytes, f)
    }
}

impl<'a> From<&'a [u8]> for Bytes<'a> {
    fn from(bytes: &'a [u8]) -> Self {
        Bytes::new(bytes)
    }
}

impl<'a> From<Bytes<'a>> for &'a [u8] {
    fn from(wrapper: Bytes<'a>) -> &'a [u8] {
        wrapper.bytes
    }
}

impl<'a> ops::Deref for Bytes<'a> {
    type Target = [u8];

    fn deref(&self) -> &[u8] {
        self.bytes
    }
}

impl<'a> Serialize for Bytes<'a> {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: Serializer
    {
        serializer.serialize_bytes(self.bytes)
    }
}

struct BytesVisitor;

impl<'de> Visitor<'de> for BytesVisitor {
    type Value = Bytes<'de>;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a borrowed byte array")
    }

    #[inline]
    fn visit_borrowed_bytes<E>(self, v: &'de [u8]) -> Result<Bytes<'de>, E>
        where E: Error
    {
        Ok(Bytes::from(v))
    }

    #[inline]
    fn visit_borrowed_str<E>(self, v: &'de str) -> Result<Bytes<'de>, E>
        where E: Error
    {
        Ok(Bytes::from(v.as_bytes()))
    }
}

impl<'a, 'de: 'a> Deserialize<'de> for Bytes<'a> {
    #[inline]
    fn deserialize<D>(deserializer: D) -> Result<Bytes<'a>, D::Error>
        where D: Deserializer<'de>
    {
        deserializer.deserialize_bytes(BytesVisitor)
    }
}

//////////////////////////////////////////////////////////////////////////////

#[cfg(any(feature = "std", feature = "alloc"))]
mod bytebuf {
    #[cfg(feature = "std")]
    use std::{cmp, fmt, ops};

    #[cfg(not(feature = "std"))]
    use core::{cmp, fmt, ops};

    use self::fmt::Debug;

    #[cfg(feature = "alloc")]
    use alloc::{String, Vec};

    use serde::ser::{Serialize, Serializer};
    use serde::de::{Deserialize, Deserializer, Visitor, SeqAccess, Error};

    /// Wrapper around `Vec<u8>` to serialize and deserialize efficiently.
    ///
    /// ```rust
    /// extern crate bincode;
    /// extern crate serde_bytes;
    ///
    /// use std::collections::HashMap;
    /// use std::io;
    ///
    /// use serde_bytes::ByteBuf;
    ///
    /// fn deserialize_bytebufs() -> Result<(), bincode::Error> {
    ///     let example_data = [
    ///         2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 116,
    ///         119, 111, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 111, 110, 101];
    ///
    ///     let map: HashMap<u32, ByteBuf> =
    ///         bincode::deserialize_from(&mut &example_data[..], bincode::Infinite)?;
    ///
    ///     println!("{:?}", map);
    ///
    ///     Ok(())
    /// }
    /// #
    /// # fn main() {
    /// #     deserialize_bytebufs().unwrap();
    /// # }
    /// ```
    #[derive(Clone, Default, Eq, Hash, PartialEq, PartialOrd, Ord)]
    pub struct ByteBuf {
        bytes: Vec<u8>,
    }

    impl ByteBuf {
        /// Construct a new, empty `ByteBuf`.
        pub fn new() -> Self {
            ByteBuf::from(Vec::new())
        }

        /// Construct a new, empty `ByteBuf` with the specified capacity.
        pub fn with_capacity(cap: usize) -> Self {
            ByteBuf::from(Vec::with_capacity(cap))
        }

        /// Wrap existing bytes in a `ByteBuf`.
        pub fn from<T: Into<Vec<u8>>>(bytes: T) -> Self {
            ByteBuf { bytes: bytes.into() }
        }
    }

    impl Debug for ByteBuf {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            Debug::fmt(&self.bytes, f)
        }
    }

    impl From<ByteBuf> for Vec<u8> {
        fn from(wrapper: ByteBuf) -> Vec<u8> {
            wrapper.bytes
        }
    }

    impl From<Vec<u8>> for ByteBuf {
        fn from(bytes: Vec<u8>) -> Self {
            ByteBuf::from(bytes)
        }
    }

    impl AsRef<Vec<u8>> for ByteBuf {
        fn as_ref(&self) -> &Vec<u8> {
            &self.bytes
        }
    }

    impl AsRef<[u8]> for ByteBuf {
        fn as_ref(&self) -> &[u8] {
            &self.bytes
        }
    }

    impl AsMut<Vec<u8>> for ByteBuf {
        fn as_mut(&mut self) -> &mut Vec<u8> {
            &mut self.bytes
        }
    }

    impl AsMut<[u8]> for ByteBuf {
        fn as_mut(&mut self) -> &mut [u8] {
            &mut self.bytes
        }
    }

    impl ops::Deref for ByteBuf {
        type Target = [u8];

        fn deref(&self) -> &[u8] {
            &self.bytes[..]
        }
    }

    impl ops::DerefMut for ByteBuf {
        fn deref_mut(&mut self) -> &mut [u8] {
            &mut self.bytes[..]
        }
    }

    impl Serialize for ByteBuf {
        #[inline]
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where S: Serializer
        {
            serializer.serialize_bytes(&self.bytes)
        }
    }

    struct ByteBufVisitor;

    impl<'de> Visitor<'de> for ByteBufVisitor {
        type Value = ByteBuf;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("byte array")
        }

        #[inline]
        fn visit_seq<V>(self, mut visitor: V) -> Result<ByteBuf, V::Error>
            where V: SeqAccess<'de>
        {
            let len = cmp::min(visitor.size_hint().unwrap_or(0), 4096);
            let mut values = Vec::with_capacity(len);

            while let Some(value) = try!(visitor.next_element()) {
                values.push(value);
            }

            Ok(ByteBuf::from(values))
        }

        #[inline]
        fn visit_bytes<E>(self, v: &[u8]) -> Result<ByteBuf, E>
            where E: Error
        {
            Ok(ByteBuf::from(v))
        }

        #[inline]
        fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<ByteBuf, E>
            where E: Error
        {
            Ok(ByteBuf::from(v))
        }

        #[inline]
        fn visit_str<E>(self, v: &str) -> Result<ByteBuf, E>
            where E: Error
        {
            Ok(ByteBuf::from(v))
        }

        #[inline]
        fn visit_string<E>(self, v: String) -> Result<ByteBuf, E>
            where E: Error
        {
            Ok(ByteBuf::from(v))
        }
    }

    impl<'de> Deserialize<'de> for ByteBuf {
        #[inline]
        fn deserialize<D>(deserializer: D) -> Result<ByteBuf, D::Error>
            where D: Deserializer<'de>
        {
            deserializer.deserialize_byte_buf(ByteBufVisitor)
        }
    }
}
