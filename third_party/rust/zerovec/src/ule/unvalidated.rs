// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::VarULE;
use crate::{map::ZeroMapKV, VarZeroSlice, VarZeroVec, ZeroVecError};
use alloc::boxed::Box;
use core::fmt;
use core::ops::Deref;

/// A byte slice that is expected to be a UTF-8 string but does not enforce that invariant.
///
/// Use this type instead of `str` if you don't need to enforce UTF-8 during deserialization. For
/// example, strings that are keys of a map don't need to ever be reified as `str`s.
///
/// [`UnvalidatedStr`] derefs to `[u8]`. To obtain a `str`, use [`Self::try_as_str()`].
///
/// The main advantage of this type over `[u8]` is that it serializes as a string in
/// human-readable formats like JSON.
///
/// # Examples
///
/// Using an [`UnvalidatedStr`] as the key of a [`ZeroMap`]:
///
/// ```
/// use zerovec::ule::UnvalidatedStr;
/// use zerovec::ZeroMap;
///
/// let map: ZeroMap<UnvalidatedStr, usize> = [
///     (UnvalidatedStr::from_str("abc"), 11),
///     (UnvalidatedStr::from_str("def"), 22),
///     (UnvalidatedStr::from_str("ghi"), 33),
/// ]
/// .into_iter()
/// .collect();
///
/// let key = "abc";
/// let value = map.get_copied_by(|uvstr| uvstr.as_bytes().cmp(key.as_bytes()));
/// assert_eq!(Some(11), value);
/// ```
///
/// [`ZeroMap`]: crate::ZeroMap
#[repr(transparent)]
#[derive(PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_structs)] // transparent newtype
pub struct UnvalidatedStr([u8]);

impl fmt::Debug for UnvalidatedStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Debug as a string if possible
        match self.try_as_str() {
            Ok(s) => fmt::Debug::fmt(s, f),
            Err(_) => fmt::Debug::fmt(&self.0, f),
        }
    }
}

impl UnvalidatedStr {
    /// Create a [`UnvalidatedStr`] from a byte slice.
    #[inline]
    pub const fn from_bytes(other: &[u8]) -> &Self {
        // Safety: UnvalidatedStr is transparent over [u8]
        unsafe { core::mem::transmute(other) }
    }

    /// Create a [`UnvalidatedStr`] from a string slice.
    #[inline]
    pub const fn from_str(s: &str) -> &Self {
        Self::from_bytes(s.as_bytes())
    }

    /// Create a [`UnvalidatedStr`] from boxed bytes.
    #[inline]
    pub fn from_boxed_bytes(other: Box<[u8]>) -> Box<Self> {
        // Safety: UnvalidatedStr is transparent over [u8]
        unsafe { core::mem::transmute(other) }
    }

    /// Create a [`UnvalidatedStr`] from a boxed `str`.
    #[inline]
    pub fn from_boxed_str(other: Box<str>) -> Box<Self> {
        Self::from_boxed_bytes(other.into_boxed_bytes())
    }

    /// Get the bytes from a [`UnvalidatedStr].
    #[inline]
    pub const fn as_bytes(&self) -> &[u8] {
        &self.0
    }

    /// Attempt to convert a [`UnvalidatedStr`] to a `str`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::ule::UnvalidatedStr;
    ///
    /// static A: &UnvalidatedStr = UnvalidatedStr::from_bytes(b"abc");
    ///
    /// let b = A.try_as_str().unwrap();
    /// assert_eq!(b, "abc");
    /// ```
    // Note: this is const starting in 1.63
    #[inline]
    pub fn try_as_str(&self) -> Result<&str, core::str::Utf8Error> {
        core::str::from_utf8(&self.0)
    }
}

impl<'a> From<&'a str> for &'a UnvalidatedStr {
    #[inline]
    fn from(other: &'a str) -> Self {
        UnvalidatedStr::from_str(other)
    }
}

impl From<Box<str>> for Box<UnvalidatedStr> {
    #[inline]
    fn from(other: Box<str>) -> Self {
        UnvalidatedStr::from_boxed_str(other)
    }
}

impl Deref for UnvalidatedStr {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<'a> ZeroMapKV<'a> for UnvalidatedStr {
    type Container = VarZeroVec<'a, UnvalidatedStr>;
    type Slice = VarZeroSlice<UnvalidatedStr>;
    type GetType = UnvalidatedStr;
    type OwnedType = Box<UnvalidatedStr>;
}

// Safety (based on the safety checklist on the VarULE trait):
//  1. UnvalidatedStr does not include any uninitialized or padding bytes (transparent over a ULE)
//  2. UnvalidatedStr is aligned to 1 byte (transparent over a ULE)
//  3. The impl of `validate_byte_slice()` returns an error if any byte is not valid (impossible)
//  4. The impl of `validate_byte_slice()` returns an error if the slice cannot be used in its entirety (impossible)
//  5. The impl of `from_byte_slice_unchecked()` returns a reference to the same data (returns the argument directly)
//  6. All other methods are defaulted
//  7. `[T]` byte equality is semantic equality (transparent over a ULE)
unsafe impl VarULE for UnvalidatedStr {
    #[inline]
    fn validate_byte_slice(_: &[u8]) -> Result<(), ZeroVecError> {
        Ok(())
    }
    #[inline]
    unsafe fn from_byte_slice_unchecked(bytes: &[u8]) -> &Self {
        UnvalidatedStr::from_bytes(bytes)
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
#[cfg(feature = "serde")]
impl serde::Serialize for UnvalidatedStr {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use serde::ser::Error;
        let s = self
            .try_as_str()
            .map_err(|_| S::Error::custom("invalid UTF-8 in UnvalidatedStr"))?;
        if serializer.is_human_readable() {
            serializer.serialize_str(s)
        } else {
            serializer.serialize_bytes(s.as_bytes())
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for Box<UnvalidatedStr> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            let boxed_str = Box::<str>::deserialize(deserializer)?;
            Ok(UnvalidatedStr::from_boxed_str(boxed_str))
        } else {
            let boxed_bytes = Box::<[u8]>::deserialize(deserializer)?;
            Ok(UnvalidatedStr::from_boxed_bytes(boxed_bytes))
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
#[cfg(feature = "serde")]
impl<'de, 'a> serde::Deserialize<'de> for &'a UnvalidatedStr
where
    'de: 'a,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            let s = <&str>::deserialize(deserializer)?;
            Ok(UnvalidatedStr::from_str(s))
        } else {
            let bytes = <&[u8]>::deserialize(deserializer)?;
            Ok(UnvalidatedStr::from_bytes(bytes))
        }
    }
}
