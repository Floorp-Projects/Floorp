use core::cmp::Ordering;
use core::fmt::{self, Debug};
use core::hash::{Hash, Hasher};
use core::ops::{Deref, DerefMut};

#[cfg(feature = "alloc")]
use alloc::borrow::ToOwned;

#[cfg(feature = "alloc")]
use alloc::boxed::Box;

#[cfg(any(feature = "std", feature = "alloc"))]
use crate::ByteBuf;

use serde::de::{Deserialize, Deserializer, Error, Visitor};
use serde::ser::{Serialize, Serializer};

/// Wrapper around `[u8]` to serialize and deserialize efficiently.
///
/// ```
/// use std::collections::HashMap;
/// use std::io;
///
/// use serde_bytes::Bytes;
///
/// fn print_encoded_cache() -> bincode::Result<()> {
///     let mut cache = HashMap::new();
///     cache.insert(3, Bytes::new(b"three"));
///     cache.insert(2, Bytes::new(b"two"));
///     cache.insert(1, Bytes::new(b"one"));
///
///     bincode::serialize_into(&mut io::stdout(), &cache)
/// }
/// #
/// # fn main() {
/// #     print_encoded_cache().unwrap();
/// # }
/// ```
#[derive(Eq, Ord)]
#[repr(C)]
pub struct Bytes {
    bytes: [u8],
}

impl Bytes {
    /// Wrap an existing `&[u8]`.
    pub fn new(bytes: &[u8]) -> &Self {
        unsafe { &*(bytes as *const [u8] as *const Bytes) }
    }
}

impl Debug for Bytes {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        Debug::fmt(&self.bytes, f)
    }
}

impl AsRef<[u8]> for Bytes {
    fn as_ref(&self) -> &[u8] {
        &self.bytes
    }
}

impl AsMut<[u8]> for Bytes {
    fn as_mut(&mut self) -> &mut [u8] {
        &mut self.bytes
    }
}

impl Deref for Bytes {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.bytes
    }
}

impl DerefMut for Bytes {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.bytes
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl ToOwned for Bytes {
    type Owned = ByteBuf;

    fn to_owned(&self) -> Self::Owned {
        ByteBuf::from(&self.bytes)
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl From<Box<[u8]>> for Box<Bytes> {
    fn from(bytes: Box<[u8]>) -> Self {
        unsafe { Box::from_raw(Box::into_raw(bytes) as *mut Bytes) }
    }
}

impl<'a> Default for &'a Bytes {
    fn default() -> Self {
        Bytes::new(&[])
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl Default for Box<Bytes> {
    fn default() -> Self {
        ByteBuf::new().into_boxed_bytes()
    }
}

impl<Rhs> PartialEq<Rhs> for Bytes
where
    Rhs: ?Sized + AsRef<[u8]>,
{
    fn eq(&self, other: &Rhs) -> bool {
        self.as_ref().eq(other.as_ref())
    }
}

impl<Rhs> PartialOrd<Rhs> for Bytes
where
    Rhs: ?Sized + AsRef<[u8]>,
{
    fn partial_cmp(&self, other: &Rhs) -> Option<Ordering> {
        self.as_ref().partial_cmp(other.as_ref())
    }
}

impl Hash for Bytes {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.bytes.hash(state);
    }
}

impl<'a> IntoIterator for &'a Bytes {
    type Item = &'a u8;
    type IntoIter = <&'a [u8] as IntoIterator>::IntoIter;

    fn into_iter(self) -> Self::IntoIter {
        self.bytes.iter()
    }
}

impl<'a> IntoIterator for &'a mut Bytes {
    type Item = &'a mut u8;
    type IntoIter = <&'a mut [u8] as IntoIterator>::IntoIter;

    fn into_iter(self) -> Self::IntoIter {
        self.bytes.iter_mut()
    }
}

impl Serialize for Bytes {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(&self.bytes)
    }
}

struct BytesVisitor;

impl<'de> Visitor<'de> for BytesVisitor {
    type Value = &'de Bytes;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a borrowed byte array")
    }

    fn visit_borrowed_bytes<E>(self, v: &'de [u8]) -> Result<Self::Value, E>
    where
        E: Error,
    {
        Ok(Bytes::new(v))
    }

    fn visit_borrowed_str<E>(self, v: &'de str) -> Result<Self::Value, E>
    where
        E: Error,
    {
        Ok(Bytes::new(v.as_bytes()))
    }
}

impl<'a, 'de: 'a> Deserialize<'de> for &'a Bytes {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_bytes(BytesVisitor)
    }
}
