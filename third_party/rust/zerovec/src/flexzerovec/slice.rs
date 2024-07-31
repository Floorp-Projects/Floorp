// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::FlexZeroVec;
use crate::ZeroVecError;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::fmt;
use core::mem;
use core::ops::Range;

const USIZE_WIDTH: usize = mem::size_of::<usize>();

/// A zero-copy "slice" that efficiently represents `[usize]`.
#[repr(C, packed)]
pub struct FlexZeroSlice {
    // Hard Invariant: 1 <= width <= USIZE_WIDTH (which is target_pointer_width)
    // Soft Invariant: width == the width of the largest element
    width: u8,
    // Hard Invariant: data.len() % width == 0
    data: [u8],
}

impl fmt::Debug for FlexZeroSlice {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_vec().fmt(f)
    }
}

impl PartialEq for FlexZeroSlice {
    fn eq(&self, other: &Self) -> bool {
        self.width == other.width && self.data == other.data
    }
}
impl Eq for FlexZeroSlice {}

/// Helper function to decode a little-endian "chunk" (byte slice of a specific length)
/// into a `usize`. We cannot call `usize::from_le_bytes` directly because that function
/// requires the high bits to be set to 0.
#[inline]
pub(crate) fn chunk_to_usize(chunk: &[u8], width: usize) -> usize {
    debug_assert_eq!(chunk.len(), width);
    let mut bytes = [0; USIZE_WIDTH];
    #[allow(clippy::indexing_slicing)] // protected by debug_assert above
    bytes[0..width].copy_from_slice(chunk);
    usize::from_le_bytes(bytes)
}

impl FlexZeroSlice {
    /// Constructs a new empty [`FlexZeroSlice`].
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroSlice;
    ///
    /// const EMPTY_SLICE: &FlexZeroSlice = FlexZeroSlice::new_empty();
    ///
    /// assert!(EMPTY_SLICE.is_empty());
    /// assert_eq!(EMPTY_SLICE.len(), 0);
    /// assert_eq!(EMPTY_SLICE.first(), None);
    /// ```
    #[inline]
    pub const fn new_empty() -> &'static Self {
        const ARR: &[u8] = &[1u8];
        // Safety: The slice is a valid empty `FlexZeroSlice`
        unsafe { Self::from_byte_slice_unchecked(ARR) }
    }

    /// Safely constructs a [`FlexZeroSlice`] from a byte array.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroSlice;
    ///
    /// const FZS: &FlexZeroSlice = match FlexZeroSlice::parse_byte_slice(&[
    ///     2, // width
    ///     0x42, 0x00, // first value
    ///     0x07, 0x09, // second value
    ///     0xFF, 0xFF, // third value
    /// ]) {
    ///     Ok(v) => v,
    ///     Err(_) => panic!("invalid bytes"),
    /// };
    ///
    /// assert!(!FZS.is_empty());
    /// assert_eq!(FZS.len(), 3);
    /// assert_eq!(FZS.first(), Some(0x0042));
    /// assert_eq!(FZS.get(0), Some(0x0042));
    /// assert_eq!(FZS.get(1), Some(0x0907));
    /// assert_eq!(FZS.get(2), Some(0xFFFF));
    /// assert_eq!(FZS.get(3), None);
    /// assert_eq!(FZS.last(), Some(0xFFFF));
    /// ```
    pub const fn parse_byte_slice(bytes: &[u8]) -> Result<&Self, ZeroVecError> {
        let (width_u8, data) = match bytes.split_first() {
            Some(v) => v,
            None => {
                return Err(ZeroVecError::InvalidLength {
                    ty: "FlexZeroSlice",
                    len: 0,
                })
            }
        };
        let width = *width_u8 as usize;
        if width < 1 || width > USIZE_WIDTH {
            return Err(ZeroVecError::ParseError {
                ty: "FlexZeroSlice",
            });
        }
        if data.len() % width != 0 {
            return Err(ZeroVecError::InvalidLength {
                ty: "FlexZeroSlice",
                len: bytes.len(),
            });
        }
        // Safety: All hard invariants have been checked.
        // Note: The soft invariant requires a linear search that we don't do here.
        Ok(unsafe { Self::from_byte_slice_unchecked(bytes) })
    }

    /// Constructs a [`FlexZeroSlice`] without checking invariants.
    ///
    /// # Panics
    ///
    /// Panics if `bytes` is empty.
    ///
    /// # Safety
    ///
    /// Must be called on a valid [`FlexZeroSlice`] byte array.
    #[inline]
    pub const unsafe fn from_byte_slice_unchecked(bytes: &[u8]) -> &Self {
        // Safety: The DST of FlexZeroSlice is a pointer to the `width` element and has a metadata
        // equal to the length of the `data` field, which will be one less than the length of the
        // overall array.
        #[allow(clippy::panic)] // panic is documented in function contract
        if bytes.is_empty() {
            panic!("from_byte_slice_unchecked called with empty slice")
        }
        let slice = core::ptr::slice_from_raw_parts(bytes.as_ptr(), bytes.len() - 1);
        &*(slice as *const Self)
    }

    #[inline]
    pub(crate) unsafe fn from_byte_slice_mut_unchecked(bytes: &mut [u8]) -> &mut Self {
        // Safety: See comments in `from_byte_slice_unchecked`
        let remainder = core::ptr::slice_from_raw_parts_mut(bytes.as_mut_ptr(), bytes.len() - 1);
        &mut *(remainder as *mut Self)
    }

    /// Returns this slice as its underlying `&[u8]` byte buffer representation.
    ///
    /// Useful for serialization.
    ///
    /// # Example
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroSlice;
    ///
    /// let bytes: &[u8] = &[2, 0xD3, 0x00, 0x19, 0x01, 0xA5, 0x01, 0xCD, 0x80];
    /// let fzv = FlexZeroSlice::parse_byte_slice(bytes).expect("valid bytes");
    ///
    /// assert_eq!(bytes, fzv.as_bytes());
    /// ```
    #[inline]
    pub fn as_bytes(&self) -> &[u8] {
        // Safety: See comments in `from_byte_slice_unchecked`
        unsafe {
            core::slice::from_raw_parts(self as *const Self as *const u8, self.data.len() + 1)
        }
    }

    /// Borrows this `FlexZeroSlice` as a [`FlexZeroVec::Borrowed`].
    #[inline]
    pub const fn as_flexzerovec(&self) -> FlexZeroVec {
        FlexZeroVec::Borrowed(self)
    }

    /// Returns the number of elements in the `FlexZeroSlice`.
    #[inline]
    pub fn len(&self) -> usize {
        self.data.len() / self.get_width()
    }

    #[inline]
    pub(crate) fn get_width(&self) -> usize {
        usize::from(self.width)
    }

    /// Returns whether there are zero elements in the `FlexZeroSlice`.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.data.len() == 0
    }

    /// Gets the element at `index`, or `None` if `index >= self.len()`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let fzv: FlexZeroVec = [22, 33].iter().copied().collect();
    /// assert_eq!(fzv.get(0), Some(22));
    /// assert_eq!(fzv.get(1), Some(33));
    /// assert_eq!(fzv.get(2), None);
    /// ```
    #[inline]
    pub fn get(&self, index: usize) -> Option<usize> {
        if index >= self.len() {
            None
        } else {
            Some(unsafe { self.get_unchecked(index) })
        }
    }

    /// Gets the element at `index` as a chunk of bytes, or `None` if `index >= self.len()`.
    #[inline]
    pub(crate) fn get_chunk(&self, index: usize) -> Option<&[u8]> {
        let w = self.get_width();
        self.data.get(index * w..index * w + w)
    }

    /// Gets the element at `index` without checking bounds.
    ///
    /// # Safety
    ///
    /// `index` must be in-range.
    #[inline]
    pub unsafe fn get_unchecked(&self, index: usize) -> usize {
        match self.width {
            1 => *self.data.get_unchecked(index) as usize,
            2 => {
                let ptr = self.data.as_ptr().add(index * 2);
                u16::from_le_bytes(core::ptr::read(ptr as *const [u8; 2])) as usize
            }
            _ => {
                let mut bytes = [0; USIZE_WIDTH];
                let w = self.get_width();
                assert!(w <= USIZE_WIDTH);
                let ptr = self.data.as_ptr().add(index * w);
                core::ptr::copy_nonoverlapping(ptr, bytes.as_mut_ptr(), w);
                usize::from_le_bytes(bytes)
            }
        }
    }

    /// Gets the first element of the slice, or `None` if the slice is empty.
    #[inline]
    pub fn first(&self) -> Option<usize> {
        let w = self.get_width();
        self.data.get(0..w).map(|chunk| chunk_to_usize(chunk, w))
    }

    /// Gets the last element of the slice, or `None` if the slice is empty.
    #[inline]
    pub fn last(&self) -> Option<usize> {
        let l = self.data.len();
        if l == 0 {
            None
        } else {
            let w = self.get_width();
            self.data
                .get(l - w..l)
                .map(|chunk| chunk_to_usize(chunk, w))
        }
    }

    /// Gets an iterator over the elements of the slice as `usize`.
    #[inline]
    pub fn iter(
        &self,
    ) -> impl DoubleEndedIterator<Item = usize> + '_ + ExactSizeIterator<Item = usize> {
        let w = self.get_width();
        self.data
            .chunks_exact(w)
            .map(move |chunk| chunk_to_usize(chunk, w))
    }

    /// Gets an iterator over pairs of elements.
    ///
    /// The second element of the final pair is `None`.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let nums: &[usize] = &[211, 281, 421, 461];
    /// let fzv: FlexZeroVec = nums.iter().copied().collect();
    ///
    /// let mut pairs_it = fzv.iter_pairs();
    ///
    /// assert_eq!(pairs_it.next(), Some((211, Some(281))));
    /// assert_eq!(pairs_it.next(), Some((281, Some(421))));
    /// assert_eq!(pairs_it.next(), Some((421, Some(461))));
    /// assert_eq!(pairs_it.next(), Some((461, None)));
    /// assert_eq!(pairs_it.next(), None);
    /// ```
    pub fn iter_pairs(&self) -> impl Iterator<Item = (usize, Option<usize>)> + '_ {
        self.iter().zip(self.iter().skip(1).map(Some).chain([None]))
    }

    /// Creates a `Vec<usize>` from a [`FlexZeroSlice`] (or `FlexZeroVec`).
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let nums: &[usize] = &[211, 281, 421, 461];
    /// let fzv: FlexZeroVec = nums.iter().copied().collect();
    /// let vec: Vec<usize> = fzv.to_vec();
    ///
    /// assert_eq!(nums, vec.as_slice());
    /// ```
    #[inline]
    pub fn to_vec(&self) -> Vec<usize> {
        self.iter().collect()
    }

    /// Binary searches a sorted `FlexZeroSlice` for the given `usize` value.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// let nums: &[usize] = &[211, 281, 421, 461];
    /// let fzv: FlexZeroVec = nums.iter().copied().collect();
    ///
    /// assert_eq!(fzv.binary_search(0), Err(0));
    /// assert_eq!(fzv.binary_search(211), Ok(0));
    /// assert_eq!(fzv.binary_search(250), Err(1));
    /// assert_eq!(fzv.binary_search(281), Ok(1));
    /// assert_eq!(fzv.binary_search(300), Err(2));
    /// assert_eq!(fzv.binary_search(421), Ok(2));
    /// assert_eq!(fzv.binary_search(450), Err(3));
    /// assert_eq!(fzv.binary_search(461), Ok(3));
    /// assert_eq!(fzv.binary_search(462), Err(4));
    /// ```
    #[inline]
    pub fn binary_search(&self, needle: usize) -> Result<usize, usize> {
        self.binary_search_by(|probe| probe.cmp(&needle))
    }

    /// Binary searches a sorted range of a `FlexZeroSlice` for the given `usize` value.
    ///
    /// The indices in the return value are relative to the start of the range.
    ///
    /// # Examples
    ///
    /// ```
    /// use zerovec::vecs::FlexZeroVec;
    ///
    /// // Make a FlexZeroVec with two sorted ranges: 0..3 and 3..5
    /// let nums: &[usize] = &[111, 222, 444, 333, 555];
    /// let fzv: FlexZeroVec = nums.iter().copied().collect();
    ///
    /// // Search in the first range:
    /// assert_eq!(fzv.binary_search_in_range(0, 0..3), Some(Err(0)));
    /// assert_eq!(fzv.binary_search_in_range(111, 0..3), Some(Ok(0)));
    /// assert_eq!(fzv.binary_search_in_range(199, 0..3), Some(Err(1)));
    /// assert_eq!(fzv.binary_search_in_range(222, 0..3), Some(Ok(1)));
    /// assert_eq!(fzv.binary_search_in_range(399, 0..3), Some(Err(2)));
    /// assert_eq!(fzv.binary_search_in_range(444, 0..3), Some(Ok(2)));
    /// assert_eq!(fzv.binary_search_in_range(999, 0..3), Some(Err(3)));
    ///
    /// // Search in the second range:
    /// assert_eq!(fzv.binary_search_in_range(0, 3..5), Some(Err(0)));
    /// assert_eq!(fzv.binary_search_in_range(333, 3..5), Some(Ok(0)));
    /// assert_eq!(fzv.binary_search_in_range(399, 3..5), Some(Err(1)));
    /// assert_eq!(fzv.binary_search_in_range(555, 3..5), Some(Ok(1)));
    /// assert_eq!(fzv.binary_search_in_range(999, 3..5), Some(Err(2)));
    ///
    /// // Out-of-bounds range:
    /// assert_eq!(fzv.binary_search_in_range(0, 4..6), None);
    /// ```
    #[inline]
    pub fn binary_search_in_range(
        &self,
        needle: usize,
        range: Range<usize>,
    ) -> Option<Result<usize, usize>> {
        self.binary_search_in_range_by(|probe| probe.cmp(&needle), range)
    }

    /// Binary searches a sorted `FlexZeroSlice` according to a predicate function.
    #[inline]
    pub fn binary_search_by(
        &self,
        predicate: impl FnMut(usize) -> Ordering,
    ) -> Result<usize, usize> {
        debug_assert!(self.len() <= self.data.len());
        // Safety: self.len() <= self.data.len()
        let scaled_slice = unsafe { self.data.get_unchecked(0..self.len()) };
        self.binary_search_impl(predicate, scaled_slice)
    }

    /// Binary searches a sorted range of a `FlexZeroSlice` according to a predicate function.
    ///
    /// The indices in the return value are relative to the start of the range.
    #[inline]
    pub fn binary_search_in_range_by(
        &self,
        predicate: impl FnMut(usize) -> Ordering,
        range: Range<usize>,
    ) -> Option<Result<usize, usize>> {
        // Note: We need to check bounds separately, since `self.data.get(range)` does not return
        // bounds errors, since it is indexing directly into the upscaled data array
        if range.start > self.len() || range.end > self.len() {
            return None;
        }
        let scaled_slice = self.data.get(range)?;
        Some(self.binary_search_impl(predicate, scaled_slice))
    }

    /// Binary searches a `FlexZeroSlice` by its indices.
    ///
    /// The `predicate` function is passed in-bounds indices into the `FlexZeroSlice`.
    #[inline]
    pub fn binary_search_with_index(
        &self,
        predicate: impl FnMut(usize) -> Ordering,
    ) -> Result<usize, usize> {
        debug_assert!(self.len() <= self.data.len());
        // Safety: self.len() <= self.data.len()
        let scaled_slice = unsafe { self.data.get_unchecked(0..self.len()) };
        self.binary_search_with_index_impl(predicate, scaled_slice)
    }

    /// Binary searches a range of a `FlexZeroSlice` by its indices.
    ///
    /// The `predicate` function is passed in-bounds indices into the `FlexZeroSlice`, which are
    /// relative to the start of the entire slice.
    ///
    /// The indices in the return value are relative to the start of the range.
    #[inline]
    pub fn binary_search_in_range_with_index(
        &self,
        predicate: impl FnMut(usize) -> Ordering,
        range: Range<usize>,
    ) -> Option<Result<usize, usize>> {
        // Note: We need to check bounds separately, since `self.data.get(range)` does not return
        // bounds errors, since it is indexing directly into the upscaled data array
        if range.start > self.len() || range.end > self.len() {
            return None;
        }
        let scaled_slice = self.data.get(range)?;
        Some(self.binary_search_with_index_impl(predicate, scaled_slice))
    }

    /// # Safety
    ///
    /// `scaled_slice` must be a subslice of `self.data`
    #[inline]
    fn binary_search_impl(
        &self,
        mut predicate: impl FnMut(usize) -> Ordering,
        scaled_slice: &[u8],
    ) -> Result<usize, usize> {
        self.binary_search_with_index_impl(
            |index| {
                // Safety: The contract of `binary_search_with_index_impl` says `index` is in bounds
                let actual_probe = unsafe { self.get_unchecked(index) };
                predicate(actual_probe)
            },
            scaled_slice,
        )
    }

    /// `predicate` is passed a valid index as an argument.
    ///
    /// # Safety
    ///
    /// `scaled_slice` must be a subslice of `self.data`
    fn binary_search_with_index_impl(
        &self,
        mut predicate: impl FnMut(usize) -> Ordering,
        scaled_slice: &[u8],
    ) -> Result<usize, usize> {
        // This code is an absolute atrocity. This code is not a place of honor. This
        // code is known to the State of California to cause cancer.
        //
        // Unfortunately, the stdlib's `binary_search*` functions can only operate on slices.
        // We do not have a slice. We have something we can .get() and index on, but that is not
        // a slice.
        //
        // The `binary_search*` functions also do not have a variant where they give you the element's
        // index, which we could otherwise use to directly index `self`.
        // We do have `self.indices`, but these are indices into a byte buffer, which cannot in
        // isolation be used to recoup the logical index of the element they refer to.
        //
        // However, `binary_search_by()` provides references to the elements of the slice being iterated.
        // Since the layout of Rust slices is well-defined, we can do pointer arithmetic on these references
        // to obtain the index being used by the search.
        //
        // It's worth noting that the slice we choose to search is irrelevant, as long as it has the appropriate
        // length. `self.indices` is defined to have length `self.len()`, so it is convenient to use
        // here and does not require additional allocations.
        //
        // The alternative to doing this is to implement our own binary search. This is significantly less fun.

        // Note: We always use zero_index relative to the whole indices array, even if we are
        // only searching a subslice of it.
        let zero_index = self.data.as_ptr() as *const _ as usize;
        scaled_slice.binary_search_by(|probe: &_| {
            // Note: `scaled_slice` is a slice of u8
            let index = probe as *const _ as usize - zero_index;
            predicate(index)
        })
    }
}

#[inline]
pub(crate) fn get_item_width(item_bytes: &[u8; USIZE_WIDTH]) -> usize {
    USIZE_WIDTH - item_bytes.iter().rev().take_while(|b| **b == 0).count()
}

/// Pre-computed information about a pending insertion operation.
///
/// Do not create one of these directly; call `get_insert_info()`.
pub(crate) struct InsertInfo {
    /// The bytes to be inserted, with zero-fill.
    pub item_bytes: [u8; USIZE_WIDTH],
    /// The new item width after insertion.
    pub new_width: usize,
    /// The new number of items in the vector: self.len() after insertion.
    pub new_count: usize,
    /// The new number of bytes required for the entire slice (self.data.len() + 1).
    pub new_bytes_len: usize,
}

impl FlexZeroSlice {
    /// Compute the [`InsertInfo`] for inserting the specified item anywhere into the vector.
    ///
    /// # Panics
    ///
    /// Panics if inserting the element would require allocating more than `usize::MAX` bytes.
    pub(crate) fn get_insert_info(&self, new_item: usize) -> InsertInfo {
        let item_bytes = new_item.to_le_bytes();
        let item_width = get_item_width(&item_bytes);
        let old_width = self.get_width();
        let new_width = core::cmp::max(old_width, item_width);
        let new_count = 1 + (self.data.len() / old_width);
        #[allow(clippy::unwrap_used)] // panic is documented in function contract
        let new_bytes_len = new_count
            .checked_mul(new_width)
            .unwrap()
            .checked_add(1)
            .unwrap();
        InsertInfo {
            item_bytes,
            new_width,
            new_count,
            new_bytes_len,
        }
    }

    /// This function should be called on a slice with a data array `new_data_len` long
    /// which previously held `new_count - 1` elements.
    ///
    /// After calling this function, all bytes in the slice will have been written.
    pub(crate) fn insert_impl(&mut self, insert_info: InsertInfo, insert_index: usize) {
        let InsertInfo {
            item_bytes,
            new_width,
            new_count,
            new_bytes_len,
        } = insert_info;
        debug_assert!(new_width <= USIZE_WIDTH);
        debug_assert!(new_width >= self.get_width());
        debug_assert!(insert_index < new_count);
        debug_assert_eq!(new_bytes_len, new_count * new_width + 1);
        debug_assert_eq!(new_bytes_len, self.data.len() + 1);
        // For efficiency, calculate how many items we can skip copying.
        let lower_i = if new_width == self.get_width() {
            insert_index
        } else {
            0
        };
        // Copy elements starting from the end into the new empty section of the vector.
        // Note: We could copy fully in place, but we need to set 0 bytes for the high bytes,
        // so we stage the new value on the stack.
        for i in (lower_i..new_count).rev() {
            let bytes_to_write = if i == insert_index {
                item_bytes
            } else {
                let j = if i > insert_index { i - 1 } else { i };
                debug_assert!(j < new_count - 1);
                // Safety: j is in range (assertion on previous line), and it has not been
                // overwritten yet since we are walking backwards.
                unsafe { self.get_unchecked(j).to_le_bytes() }
            };
            // Safety: The vector has capacity for `new_width` items at the new index, which is
            // later in the array than the bytes that we read above.
            unsafe {
                core::ptr::copy_nonoverlapping(
                    bytes_to_write.as_ptr(),
                    self.data.as_mut_ptr().add(new_width * i),
                    new_width,
                );
            }
        }
        self.width = new_width as u8;
    }
}

/// Pre-computed information about a pending removal operation.
///
/// Do not create one of these directly; call `get_remove_info()` or `get_sorted_pop_info()`.
pub(crate) struct RemoveInfo {
    /// The index of the item to be removed.
    pub remove_index: usize,
    /// The new item width after insertion.
    pub new_width: usize,
    /// The new number of items in the vector: self.len() after insertion.
    pub new_count: usize,
    /// The new number of bytes required for the entire slice (self.data.len() + 1).
    pub new_bytes_len: usize,
}

impl FlexZeroSlice {
    /// Compute the [`RemoveInfo`] for removing the item at the specified index.
    pub(crate) fn get_remove_info(&self, remove_index: usize) -> RemoveInfo {
        debug_assert!(remove_index < self.len());
        // Safety: remove_index is in range (assertion on previous line)
        let item_bytes = unsafe { self.get_unchecked(remove_index).to_le_bytes() };
        let item_width = get_item_width(&item_bytes);
        let old_width = self.get_width();
        let old_count = self.data.len() / old_width;
        let new_width = if item_width < old_width {
            old_width
        } else {
            debug_assert_eq!(old_width, item_width);
            // We might be removing the widest element. If so, we need to scale down.
            let mut largest_width = 1;
            for i in 0..old_count {
                if i == remove_index {
                    continue;
                }
                // Safety: i is in range (between 0 and old_count)
                let curr_bytes = unsafe { self.get_unchecked(i).to_le_bytes() };
                let curr_width = get_item_width(&curr_bytes);
                largest_width = core::cmp::max(curr_width, largest_width);
            }
            largest_width
        };
        let new_count = old_count - 1;
        // Note: the following line won't overflow because we are making the slice shorter.
        let new_bytes_len = new_count * new_width + 1;
        RemoveInfo {
            remove_index,
            new_width,
            new_count,
            new_bytes_len,
        }
    }

    /// Returns the [`RemoveInfo`] for removing the last element. Should be called
    /// on a slice sorted in ascending order.
    ///
    /// This is more efficient than `get_remove_info()` because it doesn't require a
    /// linear traversal of the vector in order to calculate `new_width`.
    pub(crate) fn get_sorted_pop_info(&self) -> RemoveInfo {
        debug_assert!(!self.is_empty());
        let remove_index = self.len() - 1;
        let old_count = self.len();
        let new_width = if old_count == 1 {
            1
        } else {
            // Safety: the FlexZeroSlice has at least two elements
            let largest_item = unsafe { self.get_unchecked(remove_index - 1).to_le_bytes() };
            get_item_width(&largest_item)
        };
        let new_count = old_count - 1;
        // Note: the following line won't overflow because we are making the slice shorter.
        let new_bytes_len = new_count * new_width + 1;
        RemoveInfo {
            remove_index,
            new_width,
            new_count,
            new_bytes_len,
        }
    }

    /// This function should be called on a valid slice.
    ///
    /// After calling this function, the slice data should be truncated to `new_data_len` bytes.
    pub(crate) fn remove_impl(&mut self, remove_info: RemoveInfo) {
        let RemoveInfo {
            remove_index,
            new_width,
            new_count,
            ..
        } = remove_info;
        debug_assert!(new_width <= self.get_width());
        debug_assert!(new_count < self.len());
        // For efficiency, calculate how many items we can skip copying.
        let lower_i = if new_width == self.get_width() {
            remove_index
        } else {
            0
        };
        // Copy elements starting from the beginning to compress the vector to fewer bytes.
        for i in lower_i..new_count {
            let j = if i < remove_index { i } else { i + 1 };
            // Safety: j is in range because j <= new_count < self.len()
            let bytes_to_write = unsafe { self.get_unchecked(j).to_le_bytes() };
            // Safety: The bytes are being copied to a section of the array that is not after
            // the section of the array that currently holds the bytes.
            unsafe {
                core::ptr::copy_nonoverlapping(
                    bytes_to_write.as_ptr(),
                    self.data.as_mut_ptr().add(new_width * i),
                    new_width,
                );
            }
        }
        self.width = new_width as u8;
    }
}
