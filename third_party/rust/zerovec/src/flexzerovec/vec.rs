// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::FlexZeroSlice;
use super::FlexZeroVecOwned;
use crate::ZeroVecError;
use core::cmp::Ordering;
use core::iter::FromIterator;
use core::ops::Deref;

/// A zero-copy data structure that efficiently stores integer values.
///
/// `FlexZeroVec` automatically increases or decreases its storage capacity based on the largest
/// integer stored in the vector. It therefore results in lower memory usage when smaller numbers
/// are usually stored, but larger values must sometimes also be stored.
///
/// The maximum value that can be stored in `FlexZeroVec` is `usize::MAX` on the current platform.
///
/// `FlexZeroVec` is the data structure for storing `usize` in a `ZeroMap`.
///
/// `FlexZeroVec` derefs to [`FlexZeroSlice`], which contains most of the methods.
///
/// # Examples
///
/// Storing a vec of `usize`s in a zero-copy way:
///
/// ```
/// use zerovec::vecs::FlexZeroVec;
///
/// // Create a FlexZeroVec and add a few numbers to it
/// let mut zv1 = FlexZeroVec::new();
/// zv1.to_mut().push(55);
/// zv1.to_mut().push(33);
/// zv1.to_mut().push(999);
/// assert_eq!(zv1.to_vec(), vec![55, 33, 999]);
///
/// // Convert it to bytes and back
/// let bytes = zv1.as_bytes();
/// let zv2 =
///     FlexZeroVec::parse_byte_slice(bytes).expect("bytes should round-trip");
/// assert_eq!(zv2.to_vec(), vec![55, 33, 999]);
///
/// // Verify the compact storage
/// assert_eq!(7, bytes.len());
/// assert!(matches!(zv2, FlexZeroVec::Borrowed(_)));
/// ```
///
/// Storing a map of `usize` to `usize` in a zero-copy way:
///
/// ```
/// use zerovec::ZeroMap;
///
/// // Append some values to the ZeroMap
/// let mut zm = ZeroMap::<usize, usize>::new();
/// assert!(zm.try_append(&29, &92).is_none());
/// assert!(zm.try_append(&38, &83).is_none());
/// assert!(zm.try_append(&56, &65).is_none());
/// assert_eq!(zm.len(), 3);
///
/// // Insert another value into the middle
/// assert!(zm.try_append(&47, &74).is_some());
/// assert!(zm.insert(&47, &74).is_none());
/// assert_eq!(zm.len(), 4);
///
/// // Verify that the values are correct
/// assert_eq!(zm.get_copied(&0), None);
/// assert_eq!(zm.get_copied(&29), Some(92));
/// assert_eq!(zm.get_copied(&38), Some(83));
/// assert_eq!(zm.get_copied(&47), Some(74));
/// assert_eq!(zm.get_copied(&56), Some(65));
/// assert_eq!(zm.get_copied(&usize::MAX), None);
/// ```
#[derive(Debug)]
#[non_exhaustive]
pub enum FlexZeroVec<'a> {
    Owned(FlexZeroVecOwned),
    Borrowed(&'a FlexZeroSlice),
}

impl<'a> Deref for FlexZeroVec<'a> {
    type Target = FlexZeroSlice;
    fn deref(&self) -> &Self::Target {
        match self {
            FlexZeroVec::Owned(v) => v.deref(),
            FlexZeroVec::Borrowed(v) => v,
        }
    }
}

impl<'a> AsRef<FlexZeroSlice> for FlexZeroVec<'a> {
    fn as_ref(&self) -> &FlexZeroSlice {
        self.deref()
    }
}

impl Eq for FlexZeroVec<'_> {}

impl<'a, 'b> PartialEq<FlexZeroVec<'b>> for FlexZeroVec<'a> {
    #[inline]
    fn eq(&self, other: &FlexZeroVec<'b>) -> bool {
        self.iter().eq(other.iter())
    }
}

impl<'a> Default for FlexZeroVec<'a> {
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl<'a> PartialOrd for FlexZeroVec<'a> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a> Ord for FlexZeroVec<'a> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.iter().cmp(other.iter())
    }
}

impl<'a> FlexZeroVec<'a> {
    #[inline]
    /// Creates a new, borrowed, empty `FlexZeroVec`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let zv: FlexZeroVec = FlexZeroVec::new();
    /// assert!(zv.is_empty());
    /// ```
    pub const fn new() -> Self {
        Self::Borrowed(FlexZeroSlice::new_empty())
    }

    /// Parses a `&[u8]` buffer into a `FlexZeroVec`.
    ///
    /// The bytes within the byte buffer must remain constant for the life of the FlexZeroVec.
    ///
    /// # Endianness
    ///
    /// The byte buffer must be encoded in little-endian, even if running in a big-endian
    /// environment. This ensures a consistent representation of data across platforms.
    ///
    /// # Max Value
    ///
    /// The bytes will fail to parse if the high value is greater than the capacity of `usize`
    /// on this platform. For example, a `FlexZeroVec` created on a 64-bit platform might fail
    /// to deserialize on a 32-bit platform.
    ///
    /// # Example
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let bytes: &[u8] = &[2, 0xD3, 0x00, 0x19, 0x01, 0xA5, 0x01, 0xCD, 0x01];
    /// let zv = FlexZeroVec::parse_byte_slice(bytes).expect("valid slice");
    ///
    /// assert!(matches!(zv, FlexZeroVec::Borrowed(_)));
    /// assert_eq!(zv.get(2), Some(421));
    /// ```
    pub fn parse_byte_slice(bytes: &'a [u8]) -> Result<Self, ZeroVecError> {
        let slice: &'a FlexZeroSlice = FlexZeroSlice::parse_byte_slice(bytes)?;
        Ok(Self::Borrowed(slice))
    }

    /// Converts a borrowed FlexZeroVec to an owned FlexZeroVec. No-op if already owned.
    ///
    /// # Example
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let bytes: &[u8] = &[2, 0xD3, 0x00, 0x19, 0x01, 0xA5, 0x01, 0xCD, 0x01];
    /// let zv = FlexZeroVec::parse_byte_slice(bytes).expect("valid bytes");
    /// assert!(matches!(zv, FlexZeroVec::Borrowed(_)));
    ///
    /// let owned = zv.into_owned();
    /// assert!(matches!(owned, FlexZeroVec::Owned(_)));
    /// ```
    pub fn into_owned(self) -> FlexZeroVec<'static> {
        match self {
            Self::Owned(owned) => FlexZeroVec::Owned(owned),
            Self::Borrowed(slice) => FlexZeroVec::Owned(FlexZeroVecOwned::from_slice(slice)),
        }
    }

    /// Allows the FlexZeroVec to be mutated by converting it to an owned variant, and producing
    /// a mutable [`FlexZeroVecOwned`].
    ///
    /// # Example
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let bytes: &[u8] = &[2, 0xD3, 0x00, 0x19, 0x01, 0xA5, 0x01, 0xCD, 0x01];
    /// let mut zv = FlexZeroVec::parse_byte_slice(bytes).expect("valid bytes");
    /// assert!(matches!(zv, FlexZeroVec::Borrowed(_)));
    ///
    /// zv.to_mut().push(12);
    /// assert!(matches!(zv, FlexZeroVec::Owned(_)));
    /// assert_eq!(zv.get(4), Some(12));
    /// ```
    pub fn to_mut(&mut self) -> &mut FlexZeroVecOwned {
        match self {
            Self::Owned(ref mut owned) => owned,
            Self::Borrowed(slice) => {
                *self = FlexZeroVec::Owned(FlexZeroVecOwned::from_slice(slice));
                // recursion is limited since we are guaranteed to hit the Owned branch
                self.to_mut()
            }
        }
    }

    /// Remove all elements from this FlexZeroVec and reset it to an empty borrowed state.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let mut zv: FlexZeroVec = [1, 2, 3].iter().copied().collect();
    /// assert!(!zv.is_empty());
    /// zv.clear();
    /// assert!(zv.is_empty());
    /// ```
    pub fn clear(&mut self) {
        *self = Self::Borrowed(FlexZeroSlice::new_empty())
    }
}

impl FromIterator<usize> for FlexZeroVec<'_> {
    /// Creates a [`FlexZeroVec::Owned`] from an iterator of `usize`.
    fn from_iter<I>(iter: I) -> Self
    where
        I: IntoIterator<Item = usize>,
    {
        FlexZeroVecOwned::from_iter(iter).into_flexzerovec()
    }
}

#[test]
fn test_zeromap_usize() {
    use crate::ZeroMap;

    let mut zm = ZeroMap::<usize, usize>::new();
    assert!(zm.try_append(&29, &92).is_none());
    assert!(zm.try_append(&38, &83).is_none());
    assert!(zm.try_append(&47, &74).is_none());
    assert!(zm.try_append(&56, &65).is_none());

    assert_eq!(zm.keys.get_width(), 1);
    assert_eq!(zm.values.get_width(), 1);

    assert_eq!(zm.insert(&47, &744), Some(74));
    assert_eq!(zm.values.get_width(), 2);
    assert_eq!(zm.insert(&47, &774), Some(744));
    assert_eq!(zm.values.get_width(), 2);
    assert!(zm.try_append(&1100, &1).is_none());
    assert_eq!(zm.keys.get_width(), 2);
    assert_eq!(zm.remove(&1100), Some(1));
    assert_eq!(zm.keys.get_width(), 1);

    assert_eq!(zm.get_copied(&0), None);
    assert_eq!(zm.get_copied(&29), Some(92));
    assert_eq!(zm.get_copied(&38), Some(83));
    assert_eq!(zm.get_copied(&47), Some(774));
    assert_eq!(zm.get_copied(&56), Some(65));
    assert_eq!(zm.get_copied(&usize::MAX), None);
}
