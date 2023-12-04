// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{AsULE, RawBytesULE, VarULE};
use crate::ule::EqULE;
use crate::{map::ZeroMapKV, VarZeroSlice, VarZeroVec, ZeroVecError};
use alloc::boxed::Box;
use core::cmp::Ordering;
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

/// A u8 array of little-endian data that is expected to be a Unicode scalar value, but is not
/// validated as such.
///
/// Use this type instead of `char` when you want to deal with data that is expected to be valid
/// Unicode scalar values, but you want control over when or if you validate that assumption.
///
/// # Examples
///
/// ```
/// use zerovec::ule::{RawBytesULE, UnvalidatedChar, ULE};
/// use zerovec::{ZeroSlice, ZeroVec};
///
/// // data known to be little-endian three-byte chunks of valid Unicode scalar values
/// let data = [0x68, 0x00, 0x00, 0x69, 0x00, 0x00, 0x4B, 0xF4, 0x01];
/// // ground truth expectation
/// let real = ['h', 'i', '👋'];
///
/// let chars: &ZeroSlice<UnvalidatedChar> = ZeroSlice::parse_byte_slice(&data).expect("invalid data length");
/// let parsed: Vec<_> = chars.iter().map(|c| unsafe { c.to_char_unchecked() }).collect();
/// assert_eq!(&parsed, &real);
///
/// let real_chars: ZeroVec<_> = real.iter().copied().map(UnvalidatedChar::from_char).collect();
/// let serialized_data = chars.as_bytes();
/// assert_eq!(serialized_data, &data);
/// ```
#[repr(transparent)]
#[derive(PartialEq, Eq, Clone, Copy, Hash)]
pub struct UnvalidatedChar([u8; 3]);

impl UnvalidatedChar {
    /// Create a [`UnvalidatedChar`] from a `char`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::ule::UnvalidatedChar;
    ///
    /// let a = UnvalidatedChar::from_char('a');
    /// assert_eq!(a.try_to_char().unwrap(), 'a');
    /// ```
    #[inline]
    pub const fn from_char(c: char) -> Self {
        let [u0, u1, u2, _u3] = (c as u32).to_le_bytes();
        Self([u0, u1, u2])
    }

    #[inline]
    #[doc(hidden)]
    pub const fn from_u24(c: u32) -> Self {
        let [u0, u1, u2, _u3] = c.to_le_bytes();
        Self([u0, u1, u2])
    }

    /// Attempt to convert a [`UnvalidatedChar`] to a `char`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::ule::{AsULE, UnvalidatedChar};
    ///
    /// let a = UnvalidatedChar::from_char('a');
    /// assert_eq!(a.try_to_char(), Ok('a'));
    ///
    /// let b = UnvalidatedChar::from_unaligned([0xFF, 0xFF, 0xFF].into());
    /// assert!(matches!(b.try_to_char(), Err(_)));
    /// ```
    #[inline]
    pub fn try_to_char(self) -> Result<char, core::char::CharTryFromError> {
        let [u0, u1, u2] = self.0;
        char::try_from(u32::from_le_bytes([u0, u1, u2, 0]))
    }

    /// Convert a [`UnvalidatedChar`] to a `char', returning [`char::REPLACEMENT_CHARACTER`]
    /// if the `UnvalidatedChar` does not represent a valid Unicode scalar value.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::ule::{AsULE, UnvalidatedChar};
    ///
    /// let a = UnvalidatedChar::from_unaligned([0xFF, 0xFF, 0xFF].into());
    /// assert_eq!(a.to_char_lossy(), char::REPLACEMENT_CHARACTER);
    /// ```
    #[inline]
    pub fn to_char_lossy(self) -> char {
        self.try_to_char().unwrap_or(char::REPLACEMENT_CHARACTER)
    }

    /// Convert a [`UnvalidatedChar`] to a `char` without checking that it is
    /// a valid Unicode scalar value.
    ///
    /// # Safety
    ///
    /// The `UnvalidatedChar` must be a valid Unicode scalar value in little-endian order.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::ule::UnvalidatedChar;
    ///
    /// let a = UnvalidatedChar::from_char('a');
    /// assert_eq!(unsafe { a.to_char_unchecked() }, 'a');
    /// ```
    #[inline]
    pub unsafe fn to_char_unchecked(self) -> char {
        let [u0, u1, u2] = self.0;
        char::from_u32_unchecked(u32::from_le_bytes([u0, u1, u2, 0]))
    }
}

impl RawBytesULE<3> {
    /// Converts a [`UnvalidatedChar`] to its ULE type. This is equivalent to calling
    /// [`AsULE::to_unaligned`].
    #[inline]
    pub const fn from_unvalidated_char(uc: UnvalidatedChar) -> Self {
        RawBytesULE(uc.0)
    }
}

impl AsULE for UnvalidatedChar {
    type ULE = RawBytesULE<3>;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        RawBytesULE(self.0)
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        Self(unaligned.0)
    }
}

// Safety: UnvalidatedChar is always the little-endian representation of a char,
// which corresponds to its AsULE::ULE type
unsafe impl EqULE for UnvalidatedChar {}

impl fmt::Debug for UnvalidatedChar {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Debug as a char if possible
        match self.try_to_char() {
            Ok(c) => fmt::Debug::fmt(&c, f),
            Err(_) => fmt::Debug::fmt(&self.0, f),
        }
    }
}

impl PartialOrd for UnvalidatedChar {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for UnvalidatedChar {
    // custom implementation, as derived Ord would compare lexicographically
    fn cmp(&self, other: &Self) -> Ordering {
        let [a0, a1, a2] = self.0;
        let a = u32::from_le_bytes([a0, a1, a2, 0]);
        let [b0, b1, b2] = other.0;
        let b = u32::from_le_bytes([b0, b1, b2, 0]);
        a.cmp(&b)
    }
}

impl From<char> for UnvalidatedChar {
    #[inline]
    fn from(value: char) -> Self {
        Self::from_char(value)
    }
}

impl TryFrom<UnvalidatedChar> for char {
    type Error = core::char::CharTryFromError;

    #[inline]
    fn try_from(value: UnvalidatedChar) -> Result<char, Self::Error> {
        value.try_to_char()
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
#[cfg(feature = "serde")]
impl serde::Serialize for UnvalidatedChar {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use serde::ser::Error;
        let c = self
            .try_to_char()
            .map_err(|_| S::Error::custom("invalid Unicode scalar value in UnvalidatedChar"))?;
        if serializer.is_human_readable() {
            serializer.serialize_char(c)
        } else {
            self.0.serialize(serializer)
        }
    }
}

/// This impl requires enabling the optional `serde` Cargo feature of the `zerovec` crate
#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for UnvalidatedChar {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        if deserializer.is_human_readable() {
            let c = <char>::deserialize(deserializer)?;
            Ok(UnvalidatedChar::from_char(c))
        } else {
            let bytes = <[u8; 3]>::deserialize(deserializer)?;
            Ok(UnvalidatedChar(bytes))
        }
    }
}

#[cfg(feature = "databake")]
impl databake::Bake for UnvalidatedChar {
    fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
        match self.try_to_char() {
            Ok(ch) => {
                env.insert("zerovec");
                let ch = ch.bake(env);
                databake::quote! {
                    zerovec::ule::UnvalidatedChar::from_char(#ch)
                }
            }
            Err(_) => {
                env.insert("zerovec");
                let u24 = u32::from_le_bytes([self.0[0], self.0[1], self.0[2], 0]);
                databake::quote! {
                    zerovec::ule::UnvalidatedChar::from_u24(#u24)
                }
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::ZeroVec;

    #[test]
    fn test_serde_fail() {
        let uc = UnvalidatedChar([0xFF, 0xFF, 0xFF]);
        serde_json::to_string(&uc).expect_err("serialize invalid char bytes");
        bincode::serialize(&uc).expect_err("serialize invalid char bytes");
    }

    #[test]
    fn test_serde_json() {
        let c = '🙃';
        let uc = UnvalidatedChar::from_char(c);
        let json_ser = serde_json::to_string(&uc).unwrap();

        assert_eq!(json_ser, r#""🙃""#);

        let json_de: UnvalidatedChar = serde_json::from_str(&json_ser).unwrap();

        assert_eq!(uc, json_de);
    }

    #[test]
    fn test_serde_bincode() {
        let c = '🙃';
        let uc = UnvalidatedChar::from_char(c);
        let bytes_ser = bincode::serialize(&uc).unwrap();

        assert_eq!(bytes_ser, [0x43, 0xF6, 0x01]);

        let bytes_de: UnvalidatedChar = bincode::deserialize(&bytes_ser).unwrap();

        assert_eq!(uc, bytes_de);
    }

    #[test]
    fn test_representation() {
        let chars = ['w', 'ω', '文', '𑄃', '🙃'];

        // backed by [UnvalidatedChar]
        let uvchars: Vec<_> = chars
            .iter()
            .copied()
            .map(UnvalidatedChar::from_char)
            .collect();
        // backed by [RawBytesULE<3>]
        let zvec: ZeroVec<_> = uvchars.clone().into_iter().collect();

        let ule_bytes = zvec.as_bytes();
        let uvbytes;
        unsafe {
            let ptr = &uvchars[..] as *const _ as *const u8;
            uvbytes = core::slice::from_raw_parts(ptr, ule_bytes.len());
        }

        // UnvalidatedChar is defined as little-endian, so this must be true on all platforms
        // also asserts that to_unaligned/from_unaligned are no-ops
        assert_eq!(uvbytes, ule_bytes);

        assert_eq!(
            &[119, 0, 0, 201, 3, 0, 135, 101, 0, 3, 17, 1, 67, 246, 1],
            ule_bytes
        );
    }

    #[test]
    fn test_char_bake() {
        databake::test_bake!(UnvalidatedChar, const: crate::ule::UnvalidatedChar::from_char('b'), zerovec);
        // surrogate code point
        databake::test_bake!(UnvalidatedChar, const: crate::ule::UnvalidatedChar::from_u24(55296u32), zerovec);
    }
}
