// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// The mutation operations in this file should panic to prevent undefined behavior
#![allow(clippy::unwrap_used)]
#![allow(clippy::expect_used)]
#![allow(clippy::indexing_slicing)]
#![allow(clippy::panic)]

use super::*;
use crate::ule::*;
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::any;
use core::convert::TryInto;
use core::marker::PhantomData;
use core::ops::Deref;
use core::ops::Range;
use core::{fmt, ptr, slice};

use super::components::LENGTH_WIDTH;
use super::components::MAX_INDEX;
use super::components::MAX_LENGTH;
use super::components::METADATA_WIDTH;

/// A fully-owned [`VarZeroVec`]. This type has no lifetime but has the same
/// internal buffer representation of [`VarZeroVec`], making it cheaply convertible to
/// [`VarZeroVec`] and [`VarZeroSlice`].
///
/// The `F` type parameter is a [`VarZeroVecFormat`] (see its docs for more details), which can be used to select the
/// precise format of the backing buffer with various size and performance tradeoffs. It defaults to [`Index16`].
pub struct VarZeroVecOwned<T: ?Sized, F = Index16> {
    marker: PhantomData<(Box<T>, F)>,
    // safety invariant: must parse into a valid VarZeroVecComponents
    entire_slice: Vec<u8>,
}

impl<T: ?Sized, F> Clone for VarZeroVecOwned<T, F> {
    fn clone(&self) -> Self {
        VarZeroVecOwned {
            marker: self.marker,
            entire_slice: self.entire_slice.clone(),
        }
    }
}

// The effect of a shift on the indices in the varzerovec.
#[derive(PartialEq)]
enum ShiftType {
    Insert,
    Replace,
    Remove,
}

impl<T: VarULE + ?Sized, F: VarZeroVecFormat> Deref for VarZeroVecOwned<T, F> {
    type Target = VarZeroSlice<T, F>;
    fn deref(&self) -> &VarZeroSlice<T, F> {
        self.as_slice()
    }
}

impl<T: VarULE + ?Sized, F> VarZeroVecOwned<T, F> {
    /// Construct an empty VarZeroVecOwned
    pub fn new() -> Self {
        Self {
            marker: PhantomData,
            entire_slice: Vec::new(),
        }
    }
}

impl<T: VarULE + ?Sized, F: VarZeroVecFormat> VarZeroVecOwned<T, F> {
    /// Construct a VarZeroVecOwned from a [`VarZeroSlice`] by cloning the internal data
    pub fn from_slice(slice: &VarZeroSlice<T, F>) -> Self {
        Self {
            marker: PhantomData,
            entire_slice: slice.as_bytes().into(),
        }
    }

    /// Construct a VarZeroVecOwned from a list of elements
    pub fn try_from_elements<A>(elements: &[A]) -> Result<Self, &'static str>
    where
        A: EncodeAsVarULE<T>,
    {
        Ok(Self {
            marker: PhantomData,
            // TODO(#1410): Rethink length errors in VZV.
            entire_slice: components::get_serializable_bytes::<T, A, F>(elements).ok_or(
                "Attempted to build VarZeroVec out of elements that \
                                     cumulatively are larger than a u32 in size",
            )?,
        })
    }

    /// Obtain this `VarZeroVec` as a [`VarZeroSlice`]
    pub fn as_slice(&self) -> &VarZeroSlice<T, F> {
        let slice: &[u8] = &self.entire_slice;
        unsafe {
            // safety: the slice is known to come from a valid parsed VZV
            VarZeroSlice::from_byte_slice_unchecked(slice)
        }
    }

    /// Try to allocate a buffer with enough capacity for `capacity`
    /// elements. Since `T` can take up an arbitrary size this will
    /// just allocate enough space for 4-byte Ts
    pub(crate) fn with_capacity(capacity: usize) -> Self {
        Self {
            marker: PhantomData,
            entire_slice: Vec::with_capacity(capacity * (F::INDEX_WIDTH + 4)),
        }
    }

    /// Try to reserve space for `capacity`
    /// elements. Since `T` can take up an arbitrary size this will
    /// just allocate enough space for 4-byte Ts
    pub(crate) fn reserve(&mut self, capacity: usize) {
        self.entire_slice.reserve(capacity * (F::INDEX_WIDTH + 4))
    }

    /// Get the position of a specific element in the data segment.
    ///
    /// If `idx == self.len()`, it will return the size of the data segment (where a new element would go).
    ///
    /// ## Safety
    /// `idx <= self.len()` and `self.as_encoded_bytes()` is well-formed.
    unsafe fn element_position_unchecked(&self, idx: usize) -> usize {
        let len = self.len();
        let out = if idx == len {
            self.entire_slice.len() - LENGTH_WIDTH - METADATA_WIDTH - (F::INDEX_WIDTH * len)
        } else {
            F::rawbytes_to_usize(*self.index_data(idx))
        };
        debug_assert!(
            out + LENGTH_WIDTH + METADATA_WIDTH + len * F::INDEX_WIDTH <= self.entire_slice.len()
        );
        out
    }

    /// Get the range of a specific element in the data segment.
    ///
    /// ## Safety
    /// `idx < self.len()` and `self.as_encoded_bytes()` is well-formed.
    unsafe fn element_range_unchecked(&self, idx: usize) -> core::ops::Range<usize> {
        let start = self.element_position_unchecked(idx);
        let end = self.element_position_unchecked(idx + 1);
        debug_assert!(start <= end, "{start} > {end}");
        start..end
    }

    /// Set the number of elements in the list without any checks.
    ///
    /// ## Safety
    /// No safe functions may be called until `self.as_encoded_bytes()` is well-formed.
    unsafe fn set_len(&mut self, len: usize) {
        assert!(len <= MAX_LENGTH);
        let len_bytes = len.to_le_bytes();
        self.entire_slice[0..LENGTH_WIDTH].copy_from_slice(&len_bytes[0..LENGTH_WIDTH]);
        // Double-check that the length fits in the length field
        assert_eq!(len_bytes[LENGTH_WIDTH..].iter().sum::<u8>(), 0);
    }

    fn index_range(index: usize) -> Range<usize> {
        let pos = LENGTH_WIDTH + METADATA_WIDTH + F::INDEX_WIDTH * index;
        pos..pos + F::INDEX_WIDTH
    }

    /// Return the raw bytes representing the given `index`.
    ///
    /// ## Safety
    /// The index must be valid, and self.as_encoded_bytes() must be well-formed
    unsafe fn index_data(&self, index: usize) -> &F::RawBytes {
        &F::RawBytes::from_byte_slice_unchecked(&self.entire_slice[Self::index_range(index)])[0]
    }

    /// Return the mutable slice representing the given `index`.
    ///
    /// ## Safety
    /// The index must be valid. self.as_encoded_bytes() must have allocated space
    /// for this index, but need not have its length appropriately set.
    unsafe fn index_data_mut(&mut self, index: usize) -> &mut F::RawBytes {
        let ptr = self.entire_slice.as_mut_ptr();
        let range = Self::index_range(index);

        // Doing this instead of just `get_unchecked_mut()` because it's unclear
        // if `get_unchecked_mut()` can be called out of bounds on a slice even
        // if we know the buffer is larger.
        let data = slice::from_raw_parts_mut(ptr.add(range.start), F::INDEX_WIDTH);

        &mut F::rawbytes_from_byte_slice_unchecked_mut(data)[0]
    }

    /// Shift the indices starting with and after `starting_index` by the provided `amount`.
    ///
    /// ## Safety
    /// Adding `amount` to each index after `starting_index` must not result in the slice from becoming malformed.
    /// The length of the slice must be correctly set.
    unsafe fn shift_indices(&mut self, starting_index: usize, amount: i32) {
        let len = self.len();
        let indices = F::rawbytes_from_byte_slice_unchecked_mut(
            &mut self.entire_slice[LENGTH_WIDTH + METADATA_WIDTH
                ..LENGTH_WIDTH + METADATA_WIDTH + F::INDEX_WIDTH * len],
        );
        for idx in &mut indices[starting_index..] {
            let mut new_idx = F::rawbytes_to_usize(*idx);
            if amount > 0 {
                new_idx = new_idx.checked_add(amount.try_into().unwrap()).unwrap();
            } else {
                new_idx = new_idx.checked_sub((-amount).try_into().unwrap()).unwrap();
            }
            *idx = F::usize_to_rawbytes(new_idx);
        }
    }

    /// Get this [`VarZeroVecOwned`] as a borrowed [`VarZeroVec`]
    ///
    /// If you wish to repeatedly call methods on this [`VarZeroVecOwned`],
    /// it is more efficient to perform this conversion first
    pub fn as_varzerovec<'a>(&'a self) -> VarZeroVec<'a, T, F> {
        self.as_slice().into()
    }

    /// Empty the vector
    pub fn clear(&mut self) {
        self.entire_slice.clear()
    }

    /// Consume this vector and return the backing buffer
    #[inline]
    pub fn into_bytes(self) -> Vec<u8> {
        self.entire_slice
    }

    /// Invalidate and resize the data at an index, optionally inserting or removing the index.
    /// Also updates affected indices and the length.
    /// Returns a slice to the new element data - it doesn't contain uninitialized data but its value is indeterminate.
    ///
    /// ## Safety
    /// - `index` must be a valid index, or, if `shift_type == ShiftType::Insert`, `index == self.len()` is allowed.
    /// - `new_size` musn't result in the data segment growing larger than `F::MAX_VALUE`.
    unsafe fn shift(&mut self, index: usize, new_size: usize, shift_type: ShiftType) -> &mut [u8] {
        // The format of the encoded data is:
        //  - four bytes of "len"
        //  - len*4 bytes for an array of indices
        //  - the actual data to which the indices point
        //
        // When inserting or removing an element, the size of the indices segment must be changed,
        // so the data before the target element must be shifted by 4 bytes in addition to the
        // shifting needed for the new element size.
        let len = self.len();
        let slice_len = self.entire_slice.len();

        let prev_element = match shift_type {
            ShiftType::Insert => {
                let pos = self.element_position_unchecked(index);
                // In the case of an insert, there's no previous element,
                // so it's an empty range at the new position.
                pos..pos
            }
            _ => self.element_range_unchecked(index),
        };

        // How much shifting must be done in bytes due to removal/insertion of an index.
        let index_shift: i64 = match shift_type {
            ShiftType::Insert => F::INDEX_WIDTH as i64,
            ShiftType::Replace => 0,
            ShiftType::Remove => -(F::INDEX_WIDTH as i64),
        };
        // The total shift in byte size of the owned slice.
        let shift: i64 =
            new_size as i64 - (prev_element.end - prev_element.start) as i64 + index_shift;
        let new_slice_len = slice_len.wrapping_add(shift as usize);
        if shift > 0 {
            if new_slice_len > F::MAX_VALUE as usize {
                panic!(
                    "Attempted to grow VarZeroVec to an encoded size that does not fit within the length size used by {}",
                    any::type_name::<F>()
                );
            }
            self.entire_slice.reserve(shift as usize);
        }

        // Now that we've ensured there's enough space, we can shift the data around.
        {
            // Note: There are no references introduced between pointer creation and pointer use, and all
            //       raw pointers are derived from a single &mut. This preserves pointer provenance.
            let slice_range = self.entire_slice.as_mut_ptr_range();
            let data_start = slice_range
                .start
                .add(LENGTH_WIDTH + METADATA_WIDTH + len * F::INDEX_WIDTH);
            let prev_element_p =
                data_start.add(prev_element.start)..data_start.add(prev_element.end);

            // The memory range of the affected index.
            // When inserting: where the new index goes.
            // When removing:  where the index being removed is.
            // When replacing: unused.
            let index_range = {
                let index_start = slice_range
                    .start
                    .add(LENGTH_WIDTH + METADATA_WIDTH + F::INDEX_WIDTH * index);
                index_start..index_start.add(F::INDEX_WIDTH)
            };

            unsafe fn shift_bytes(block: Range<*const u8>, to: *mut u8) {
                debug_assert!(block.end >= block.start);
                ptr::copy(block.start, to, block.end.offset_from(block.start) as usize);
            }

            if shift_type == ShiftType::Remove {
                // Move the data before the element back by 4 to remove the index.
                shift_bytes(index_range.end..prev_element_p.start, index_range.start);
            }

            // Shift data after the element to its new position.
            shift_bytes(
                prev_element_p.end..slice_range.end,
                prev_element_p
                    .start
                    .offset((new_size as i64 + index_shift) as isize),
            );

            let first_affected_index = match shift_type {
                ShiftType::Insert => {
                    // Move data before the element forward by 4 to make space for a new index.
                    shift_bytes(index_range.start..prev_element_p.start, index_range.end);

                    *self.index_data_mut(index) = F::usize_to_rawbytes(prev_element.start);
                    self.set_len(len + 1);
                    index + 1
                }
                ShiftType::Remove => {
                    self.set_len(len - 1);
                    index
                }
                ShiftType::Replace => index + 1,
            };
            // No raw pointer use should occur after this point (because of self.index_data and self.set_len).

            // Set the new slice length. This must be done after shifting data around to avoid uninitialized data.
            self.entire_slice.set_len(new_slice_len);

            // Shift the affected indices.
            self.shift_indices(first_affected_index, (shift - index_shift) as i32);
        };

        debug_assert!(self.verify_integrity());

        // Return a mut slice to the new element data.
        let element_pos = LENGTH_WIDTH
            + METADATA_WIDTH
            + self.len() * F::INDEX_WIDTH
            + self.element_position_unchecked(index);
        &mut self.entire_slice[element_pos..element_pos + new_size]
    }

    /// Checks the internal invariants of the vec to ensure safe code will not cause UB.
    /// Returns whether integrity was verified.
    ///
    /// Note: an index is valid if it doesn't point to data past the end of the slice and is
    /// less than or equal to all future indices. The length of the index segment is not part of each index.
    fn verify_integrity(&self) -> bool {
        if self.is_empty() && !self.entire_slice.is_empty() {
            return false;
        }
        let slice_len = self.entire_slice.len();
        match slice_len {
            0 => return true,
            1..=3 => return false,
            _ => (),
        }
        let len = unsafe {
            RawBytesULE::<LENGTH_WIDTH>::from_byte_slice_unchecked(
                &self.entire_slice[..LENGTH_WIDTH],
            )[0]
            .as_unsigned_int()
        };
        if len == 0 {
            // An empty vec must have an empty slice: there is only a single valid byte representation.
            return false;
        }
        if slice_len < LENGTH_WIDTH + METADATA_WIDTH + len as usize * F::INDEX_WIDTH {
            // Not enough room for the indices.
            return false;
        }
        let data_len =
            self.entire_slice.len() - LENGTH_WIDTH - METADATA_WIDTH - len as usize * F::INDEX_WIDTH;
        if data_len > MAX_INDEX {
            // The data segment is too long.
            return false;
        }

        // Test index validity.
        let indices = unsafe {
            F::RawBytes::from_byte_slice_unchecked(
                &self.entire_slice[LENGTH_WIDTH + METADATA_WIDTH
                    ..LENGTH_WIDTH + METADATA_WIDTH + len as usize * F::INDEX_WIDTH],
            )
        };
        for idx in indices {
            if F::rawbytes_to_usize(*idx) > data_len {
                // Indices must not point past the data segment.
                return false;
            }
        }
        for window in indices.windows(2) {
            if F::rawbytes_to_usize(window[0]) > F::rawbytes_to_usize(window[1]) {
                // Indices must be in non-decreasing order.
                return false;
            }
        }
        true
    }

    /// Insert an element at the end of this vector
    pub fn push<A: EncodeAsVarULE<T> + ?Sized>(&mut self, element: &A) {
        self.insert(self.len(), element)
    }

    /// Insert an element at index `idx`
    pub fn insert<A: EncodeAsVarULE<T> + ?Sized>(&mut self, index: usize, element: &A) {
        let len = self.len();
        if index > len {
            panic!("Called out-of-bounds insert() on VarZeroVec, index {index} len {len}");
        }

        let value_len = element.encode_var_ule_len();

        if len == 0 {
            let header_len = LENGTH_WIDTH + METADATA_WIDTH + F::INDEX_WIDTH;
            let cap = header_len + value_len;
            self.entire_slice.resize(cap, 0);
            self.entire_slice[0] = 1; // set length
            element.encode_var_ule_write(&mut self.entire_slice[header_len..]);
            return;
        }

        assert!(value_len < MAX_INDEX);
        unsafe {
            let place = self.shift(index, value_len, ShiftType::Insert);
            element.encode_var_ule_write(place);
        }
    }

    /// Remove the element at index `idx`
    pub fn remove(&mut self, index: usize) {
        let len = self.len();
        if index >= len {
            panic!("Called out-of-bounds remove() on VarZeroVec, index {index} len {len}");
        }
        if len == 1 {
            // This is removing the last element. Set the slice to empty to ensure all empty vecs have empty data slices.
            self.entire_slice.clear();
            return;
        }
        unsafe {
            self.shift(index, 0, ShiftType::Remove);
        }
    }

    /// Replace the element at index `idx` with another
    pub fn replace<A: EncodeAsVarULE<T> + ?Sized>(&mut self, index: usize, element: &A) {
        let len = self.len();
        if index >= len {
            panic!("Called out-of-bounds replace() on VarZeroVec, index {index} len {len}");
        }

        let value_len = element.encode_var_ule_len();

        assert!(value_len < MAX_INDEX);
        unsafe {
            let place = self.shift(index, value_len, ShiftType::Replace);
            element.encode_var_ule_write(place);
        }
    }
}

impl<T: VarULE + ?Sized, F: VarZeroVecFormat> fmt::Debug for VarZeroVecOwned<T, F>
where
    T: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        VarZeroSlice::fmt(self, f)
    }
}

impl<T: VarULE + ?Sized, F> Default for VarZeroVecOwned<T, F> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T, A, F> PartialEq<&'_ [A]> for VarZeroVecOwned<T, F>
where
    T: VarULE + ?Sized,
    T: PartialEq,
    A: AsRef<T>,
    F: VarZeroVecFormat,
{
    #[inline]
    fn eq(&self, other: &&[A]) -> bool {
        self.iter().eq(other.iter().map(|t| t.as_ref()))
    }
}

impl<'a, T: ?Sized + VarULE, F: VarZeroVecFormat> From<&'a VarZeroSlice<T, F>>
    for VarZeroVecOwned<T, F>
{
    fn from(other: &'a VarZeroSlice<T, F>) -> Self {
        Self::from_slice(other)
    }
}

#[cfg(test)]
mod test {
    use super::VarZeroVecOwned;
    #[test]
    fn test_insert_integrity() {
        let mut items: Vec<String> = Vec::new();
        let mut zerovec = VarZeroVecOwned::<str>::new();

        // Insert into an empty vec.
        items.insert(0, "1234567890".into());
        zerovec.insert(0, "1234567890");
        assert_eq!(zerovec, &*items);

        zerovec.insert(1, "foo3");
        items.insert(1, "foo3".into());
        assert_eq!(zerovec, &*items);

        // Insert at the end.
        items.insert(items.len(), "qwertyuiop".into());
        zerovec.insert(zerovec.len(), "qwertyuiop");
        assert_eq!(zerovec, &*items);

        items.insert(0, "asdfghjkl;".into());
        zerovec.insert(0, "asdfghjkl;");
        assert_eq!(zerovec, &*items);

        items.insert(2, "".into());
        zerovec.insert(2, "");
        assert_eq!(zerovec, &*items);
    }

    #[test]
    // ensure that inserting empty items works
    fn test_empty_inserts() {
        let mut items: Vec<String> = Vec::new();
        let mut zerovec = VarZeroVecOwned::<str>::new();

        // Insert into an empty vec.
        items.insert(0, "".into());
        zerovec.insert(0, "");
        assert_eq!(zerovec, &*items);

        items.insert(0, "".into());
        zerovec.insert(0, "");
        assert_eq!(zerovec, &*items);

        items.insert(0, "1234567890".into());
        zerovec.insert(0, "1234567890");
        assert_eq!(zerovec, &*items);

        items.insert(0, "".into());
        zerovec.insert(0, "");
        assert_eq!(zerovec, &*items);
    }

    #[test]
    fn test_small_insert_integrity() {
        // Tests that insert() works even when there
        // is not enough space for the new index in entire_slice.len()
        let mut items: Vec<String> = Vec::new();
        let mut zerovec = VarZeroVecOwned::<str>::new();

        // Insert into an empty vec.
        items.insert(0, "abc".into());
        zerovec.insert(0, "abc");
        assert_eq!(zerovec, &*items);

        zerovec.insert(1, "def");
        items.insert(1, "def".into());
        assert_eq!(zerovec, &*items);
    }

    #[test]
    #[should_panic]
    fn test_insert_past_end() {
        VarZeroVecOwned::<str>::new().insert(1, "");
    }

    #[test]
    fn test_remove_integrity() {
        let mut items: Vec<&str> = vec!["apples", "bananas", "eeples", "", "baneenees", "five", ""];
        let mut zerovec = VarZeroVecOwned::<str>::try_from_elements(&items).unwrap();

        for index in [0, 2, 4, 0, 1, 1, 0] {
            items.remove(index);
            zerovec.remove(index);
            assert_eq!(zerovec, &*items, "index {}, len {}", index, items.len());
        }
    }

    #[test]
    fn test_removing_last_element_clears() {
        let mut zerovec = VarZeroVecOwned::<str>::try_from_elements(&["buy some apples"]).unwrap();
        assert!(!zerovec.as_bytes().is_empty());
        zerovec.remove(0);
        assert!(zerovec.as_bytes().is_empty());
    }

    #[test]
    #[should_panic]
    fn test_remove_past_end() {
        VarZeroVecOwned::<str>::new().remove(0);
    }

    #[test]
    fn test_replace_integrity() {
        let mut items: Vec<&str> = vec!["apples", "bananas", "eeples", "", "baneenees", "five", ""];
        let mut zerovec = VarZeroVecOwned::<str>::try_from_elements(&items).unwrap();

        // Replace with an element of the same size (and the first element)
        items[0] = "blablah";
        zerovec.replace(0, "blablah");
        assert_eq!(zerovec, &*items);

        // Replace with a smaller element
        items[1] = "twily";
        zerovec.replace(1, "twily");
        assert_eq!(zerovec, &*items);

        // Replace an empty element
        items[3] = "aoeuidhtns";
        zerovec.replace(3, "aoeuidhtns");
        assert_eq!(zerovec, &*items);

        // Replace the last element
        items[6] = "0123456789";
        zerovec.replace(6, "0123456789");
        assert_eq!(zerovec, &*items);

        // Replace with an empty element
        items[2] = "";
        zerovec.replace(2, "");
        assert_eq!(zerovec, &*items);
    }

    #[test]
    #[should_panic]
    fn test_replace_past_end() {
        VarZeroVecOwned::<str>::new().replace(0, "");
    }
}
