// Copyright 2017 Matt Brubeck. See the COPYRIGHT file at the top-level
// directory of this distribution and at http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! [`SmallBitVec`] is a bit vector, a vector of single-bit values stored compactly in memory.
//!
//! SmallBitVec grows dynamically, like the standard `Vec<T>` type.  It can hold up to about one
//! word of bits inline (without a separate heap allocation).  If the number of bits exceeds this
//! inline capacity, it will allocate a buffer on the heap.
//!
//! [`SmallBitVec`]: struct.SmallBitVec.html

use std::cmp::max;
use std::fmt;
use std::hash;
use std::iter::{DoubleEndedIterator, ExactSizeIterator, FromIterator};
use std::mem::{forget, replace, size_of};
use std::ops::Range;
use std::slice;

/// A resizable bit vector, optimized for size and inline storage.
///
/// `SmallBitVec` is exactly one word wide. Depending on the required capacity, this word
/// either stores the bits inline, or it stores a pointer to a separate buffer on the heap.
pub struct SmallBitVec {
    data: usize,
}

/// Total number of bits per word.
fn inline_bits() -> u32 {
    size_of::<usize>() as u32 * 8
}

/// For an inline vector, all bits except two can be used as storage capacity:
///
/// - The rightmost bit is set to zero to signal an inline vector.
/// - The position of the rightmost nonzero bit encodes the length.
fn inline_capacity() -> u32 {
    inline_bits() - 2
}

/// The position of the nth bit of storage in an inline vector.
fn inline_index(n: u32) -> usize {
    debug_assert!(n <= inline_capacity());
    // The storage starts at the leftmost bit.
    1 << (inline_bits() - 1 - n)
}

/// If the rightmost bit of `data` is set, then the remaining bits of `data`
/// are a pointer to a heap allocation.
const HEAP_FLAG: usize = 1;

/// The allocation will contain a `Header` followed by a [Storage] buffer.
type Storage = u32;

/// The number of bits in one `Storage`.
#[inline(always)]
fn bits_per_storage() -> u32 {
    size_of::<Storage>() as u32 * 8
}

/// Data stored at the start of the heap allocation.
///
/// `Header` must have the same alignment as `Storage`.
struct Header {
    /// The number of bits in this bit vector.
    len: Storage,

    /// The number of elements in the [u32] buffer that follows this header.
    buffer_len: Storage,
}

impl Header {
    /// Create a heap allocation with enough space for a header,
    /// plus a buffer of at least `cap` bits, each initialized to `val`.
    fn new(cap: u32, len: u32, val: bool) -> *mut Header {
        let alloc_len = header_len() + buffer_len(cap);
        let init = if val { !0 } else { 0 };

        let v: Vec<Storage> = vec![init; alloc_len];

        let buffer_len = v.capacity() - header_len();
        let header_ptr = v.as_ptr() as *mut Header;

        forget(v);

        unsafe {
            (*header_ptr).len = len;
            (*header_ptr).buffer_len = buffer_len as u32;
        }
        header_ptr
    }
}

/// The number of `Storage` elements to allocate to hold a header.
fn header_len() -> usize {
    size_of::<Header>() / size_of::<Storage>()
}

/// The minimum number of `Storage` elements to hold at least `cap` bits.
fn buffer_len(cap: u32) -> usize {
    ((cap + bits_per_storage() - 1) / bits_per_storage()) as usize
}

impl SmallBitVec {
    /// Create an empty vector.
    #[inline]
    pub fn new() -> SmallBitVec {
        SmallBitVec {
            data: inline_index(0)
        }
    }

    /// Create a vector containing `len` bits, each set to `val`.
    pub fn from_elem(len: u32, val: bool) -> SmallBitVec {
        let header_ptr = Header::new(len, len, val);
        SmallBitVec {
            data: (header_ptr as usize) | HEAP_FLAG
        }
    }

    /// Create a vector with at least `cap` bits of storage.
    pub fn with_capacity(cap: u32) -> SmallBitVec {
        // Use inline storage if possible.
        if cap <= inline_capacity() {
            return SmallBitVec::new()
        }

        // Otherwise, allocate on the heap.
        let header_ptr = Header::new(cap, 0, false);
        SmallBitVec {
            data: (header_ptr as usize) | HEAP_FLAG
        }
    }

    /// The number of bits stored in this bit vector.
    #[inline]
    pub fn len(&self) -> u32 {
        if self.is_inline() {
            // The rightmost nonzero bit is a sentinel.  All bits to the left of
            // the sentinel bit are the elements of the bit vector.
            inline_bits() - self.data.trailing_zeros() - 1
        } else {
            self.header().len
        }
    }

    /// Returns `true` if this vector contains no bits.
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The number of bits that can be stored in this bit vector without re-allocating.
    #[inline]
    pub fn capacity(&self) -> u32 {
        if self.is_inline() {
            inline_capacity()
        } else {
            self.header().buffer_len * bits_per_storage()
        }
    }

    /// Get the nth bit in this bit vector.  Panics if the index is out of bounds.
    #[inline]
    pub fn get(&self, n: u32) -> bool {
        assert!(n < self.len(), "Index {} out of bounds", n);
        unsafe { self.get_unchecked(n) }
    }

    /// Get the nth bit in this bit vector, without bounds checks.
    pub unsafe fn get_unchecked(&self, n: u32) -> bool {
        if self.is_inline() {
            self.data & inline_index(n) != 0
        } else {
            let buffer = self.buffer();
            let i = (n / bits_per_storage()) as usize;
            let offset = n % bits_per_storage();
            *buffer.get_unchecked(i) & (1 << offset) != 0
        }
    }

    /// Set the nth bit in this bit vector to `val`.  Panics if the index is out of bounds.
    pub fn set(&mut self, n: u32, val: bool) {
        assert!(n < self.len(), "Index {} out of bounds", n);
        unsafe { self.set_unchecked(n, val); }
    }

    /// Set the nth bit in this bit vector to `val`, without bounds checks.
    pub unsafe fn set_unchecked(&mut self, n: u32, val: bool) {
        if self.is_inline() {
            if val {
                self.data |= inline_index(n);
            } else {
                self.data &= !inline_index(n);
            }
        } else {
            let buffer = self.buffer_mut();
            let i = (n / bits_per_storage()) as usize;
            let offset = n % bits_per_storage();
            if val {
                *buffer.get_unchecked_mut(i) |= 1 << offset;
            } else {
                *buffer.get_unchecked_mut(i) &= !(1 << offset);
            }
        }
    }

    /// Append a bit to the end of the vector.
    ///
    /// ```
    /// use smallbitvec::SmallBitVec;
    /// let mut v = SmallBitVec::new();
    /// v.push(true);
    ///
    /// assert_eq!(v.len(), 1);
    /// assert_eq!(v.get(0), true);
    /// ```
    #[inline]
    pub fn push(&mut self, val: bool) {
        let idx = self.len();
        if idx == self.capacity() {
            self.reserve(1);
        }
        unsafe {
            self.set_len(idx + 1);
            self.set_unchecked(idx, val);
        }
    }

    /// Remove the last bit from the vector and return it, if there is one.
    ///
    /// ```
    /// use smallbitvec::SmallBitVec;
    /// let mut v = SmallBitVec::new();
    /// v.push(false);
    ///
    /// assert_eq!(v.pop(), Some(false));
    /// assert_eq!(v.len(), 0);
    /// assert_eq!(v.pop(), None);
    /// ```
    pub fn pop(&mut self) -> Option<bool> {
        let old_len = self.len();
        if old_len == 0 {
            return None
        }
        unsafe {
            let val = self.get_unchecked(old_len - 1);
            self.set_len(old_len - 1);
            Some(val)
        }
    }

    /// Remove the bit at index `idx`, shifting all later bits toward the front.
    ///
    /// Panics if the index is out of bounds.
    pub fn remove(&mut self, idx: u32) {
        assert!(idx < self.len(), "Index {} out of bounds", idx);

        for i in (idx+1)..self.len() {
            unsafe {
                let next_val = self.get_unchecked(i);
                self.set_unchecked(i - 1, next_val);
            }
        }
        self.pop();
    }

    /// Remove all elements from the vector, without deallocating its buffer.
    pub fn clear(&mut self) {
        unsafe {
            self.set_len(0);
        }
    }

    /// Reserve capacity for at least `additional` more elements to be inserted.
    ///
    /// May reserve more space than requested, to avoid frequent reallocations.
    ///
    /// Panics if the new capacity overflows `u32`.
    ///
    /// Re-allocates only if `self.capacity() < self.len() + additional`.
    pub fn reserve(&mut self, additional: u32) {
        let old_cap = self.capacity();
        let new_cap = self.len().checked_add(additional).expect("capacity overflow");
        if new_cap <= old_cap {
            return
        }
        // Ensure the new capacity is at least double, to guarantee exponential growth.
        let double_cap = old_cap.saturating_mul(2);
        self.reallocate(max(new_cap, double_cap));
    }

    /// Set the length of the vector. The length must not exceed the capacity.
    ///
    /// If this makes the vector longer, then the values of its new elements
    /// are not specified.
    unsafe fn set_len(&mut self, len: u32) {
        debug_assert!(len <= self.capacity());
        if self.is_inline() {
            let sentinel = inline_index(len);
            let mask = !(sentinel - 1);
            self.data |= sentinel;
            self.data &= mask;
        } else {
            self.header_mut().len = len;
        }
    }

    /// Returns an iterator that yields the bits of the vector in order, as `bool` values.
    pub fn iter(&self) -> Iter {
        Iter { vec: self, range: 0..self.len() }
    }

    /// Returns true if all the bits in the vec are set to zero/false.
    pub fn all_false(&self) -> bool {
        let mut len = self.len();
        if len == 0 {
            return true
        }

        if self.is_inline() {
            let mask = !(inline_index(len - 1) - 1);
            self.data & mask == 0
        } else {
            for &storage in self.buffer() {
                if len >= bits_per_storage() {
                    if storage != 0 {
                        return false
                    }
                    len -= bits_per_storage();
                } else {
                    let mask = (1 << len) - 1;
                    if storage & mask != 0 {
                        return false
                    }
                    break
                }
            }
            true
        }
    }

    /// Returns true if all the bits in the vec are set to one/true.
    pub fn all_true(&self) -> bool {
        let mut len = self.len();
        if len == 0 {
            return true
        }

        if self.is_inline() {
            let mask = !(inline_index(len - 1) - 1);
            self.data & mask == mask
        } else {
            for &storage in self.buffer() {
                if len >= bits_per_storage() {
                    if storage != !0 {
                        return false
                    }
                    len -= bits_per_storage();
                } else {
                    let mask = (1 << len) - 1;
                    if storage & mask != mask {
                        return false
                    }
                    break
                }
            }
            true
        }
    }

    /// Resize the vector to have capacity for at least `cap` bits.
    ///
    /// `cap` must be at least as large as the length of the vector.
    fn reallocate(&mut self, cap: u32) {
        let old_cap = self.capacity();
        if cap <= old_cap {
            return
        }
        assert!(self.len() <= cap);

        if self.is_heap() {
            let old_buffer_len = self.header().buffer_len as usize;
            let new_buffer_len = buffer_len(cap);

            let old_alloc_len = header_len() + old_buffer_len;
            let new_alloc_len = header_len() + new_buffer_len;

            let old_ptr = self.header_raw() as *mut Storage;
            let mut v = unsafe {
                Vec::from_raw_parts(old_ptr, old_alloc_len, old_alloc_len)
            };
            v.resize(new_alloc_len, 0);
            v.shrink_to_fit();
            self.data = v.as_ptr() as usize | HEAP_FLAG;
            forget(v);

            self.header_mut().buffer_len = new_buffer_len as u32;
        } else {
            let old_self = replace(self, SmallBitVec::with_capacity(cap));
            unsafe {
                self.set_len(old_self.len());
                for i in 0..old_self.len() {
                    self.set_unchecked(i, old_self.get_unchecked(i));
                }
            }
        }
    }

    /// If the rightmost bit is set, then we treat it as inline storage.
    fn is_inline(&self) -> bool {
        self.data & HEAP_FLAG == 0
    }

    /// Otherwise, `data` is a pointer to a heap allocation.
    fn is_heap(&self) -> bool {
        !self.is_inline()
    }

    /// Get the header of a heap-allocated vector.
    fn header_raw(&self) -> *mut Header {
        assert!(self.is_heap());
        (self.data & !HEAP_FLAG) as *mut Header
    }

    fn header_mut(&mut self) -> &mut Header {
        unsafe { &mut *self.header_raw() }
    }

    fn header(&self) -> &Header {
        unsafe { &*self.header_raw() }
    }

    /// Get the buffer of a heap-allocated vector.
    fn buffer_raw(&self) -> *mut [Storage] {
        unsafe {
            let header_ptr = self.header_raw();
            let buffer_len = (*header_ptr).buffer_len as usize;
            let buffer_ptr = (header_ptr as *mut Storage)
                .offset((size_of::<Header>() / size_of::<Storage>()) as isize);
            slice::from_raw_parts_mut(buffer_ptr, buffer_len)
        }
    }

    fn buffer_mut(&self) -> &mut [Storage] {
        unsafe { &mut *self.buffer_raw() }
    }

    fn buffer(&self) -> &[Storage] {
        unsafe { &*self.buffer_raw() }
    }
}

// Trait implementations:

impl fmt::Debug for SmallBitVec {
    #[inline]
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_list().entries(self.iter().map(|b| b as u8)).finish()
    }
}

impl PartialEq for SmallBitVec {
    fn eq(&self, other: &Self) -> bool {
        // Compare by inline representation
        if self.is_inline() && other.is_inline() {
            return self.data == other.data
        }

        let len = self.len();
        if len != other.len() {
            return false
        }

        // Compare by heap representation
        if self.is_heap() && other.is_heap() {
            let buf0 = self.buffer();
            let buf1 = other.buffer();

            let full_blocks = (len / bits_per_storage()) as usize;
            let remainder = len % bits_per_storage();

            if buf0[..full_blocks] != buf1[..full_blocks] {
                return false
            }

            if remainder != 0 {
                let mask = (1 << remainder) - 1;
                if buf0[full_blocks] & mask != buf1[full_blocks] & mask {
                    return false
                }
            }
            return true
        }

        // Representations differ; fall back to bit-by-bit comparison
        Iterator::eq(self.iter(), other.iter())
    }
}

impl Eq for SmallBitVec {}

impl Drop for SmallBitVec {
    fn drop(&mut self) {
        if self.is_heap() {
            unsafe {
                let header_ptr = self.header_raw();
                let alloc_ptr = header_ptr as *mut Storage;
                let alloc_len = header_len() + (*header_ptr).buffer_len as usize;
                Vec::from_raw_parts(alloc_ptr, alloc_len, alloc_len);
            }
        }
    }
}

impl Clone for SmallBitVec {
    fn clone(&self) -> Self {
        if self.is_inline() {
            return SmallBitVec { data: self.data }
        }

        let buffer_len = self.header().buffer_len as usize;
        let alloc_len = header_len() + buffer_len;
        let ptr = self.header_raw() as *mut Storage;
        let raw_allocation = unsafe {
            slice::from_raw_parts(ptr, alloc_len)
        };

        let v = raw_allocation.to_vec();
        let header_ptr = v.as_ptr() as *mut Header;
        forget(v);
        SmallBitVec {
            data: (header_ptr as usize) | HEAP_FLAG
        }
    }
}

impl hash::Hash for SmallBitVec {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.len().hash(state);
        for b in self.iter() {
            b.hash(state);
        }
    }
}

impl Extend<bool> for SmallBitVec {
    #[inline]
    fn extend<I: IntoIterator<Item=bool>>(&mut self, iter: I) {
        let iter = iter.into_iter();

        let (min, _) = iter.size_hint();
        assert!(min <= u32::max_value() as usize, "capacity overflow");
        self.reserve(min as u32);

        for element in iter {
            self.push(element)
        }
    }
}

impl FromIterator<bool> for SmallBitVec {
    #[inline]
    fn from_iter<I: IntoIterator<Item=bool>>(iter: I) -> Self {
        let mut v = SmallBitVec::new();
        v.extend(iter);
        v
    }
}

impl IntoIterator for SmallBitVec {
    type Item = bool;
    type IntoIter = IntoIter;

    #[inline]
    fn into_iter(self) -> IntoIter {
        IntoIter { range: 0..self.len(), vec: self }
    }
}

impl<'a> IntoIterator for &'a SmallBitVec {
    type Item = bool;
    type IntoIter = Iter<'a>;

    #[inline]
    fn into_iter(self) -> Iter<'a> {
        self.iter()
    }
}

/// An iterator that borrows a SmallBitVec and yields its bits as `bool` values.
///
/// Returned from [`SmallBitVec::into_iter`][1].
///
/// [1]: struct.SmallBitVec.html#method.into_iter
pub struct IntoIter {
    vec: SmallBitVec,
    range: Range<u32>,
}

impl Iterator for IntoIter {
    type Item = bool;

    #[inline]
    fn next(&mut self) -> Option<bool> {
        self.range.next().map(|i| unsafe { self.vec.get_unchecked(i) })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.range.size_hint()
    }
}

impl DoubleEndedIterator for IntoIter {
    #[inline]
    fn next_back(&mut self) -> Option<bool> {
        self.range.next_back().map(|i| unsafe { self.vec.get_unchecked(i) })
    }
}

impl ExactSizeIterator for IntoIter {}

/// An iterator that borrows a SmallBitVec and yields its bits as `bool` values.
///
/// Returned from [`SmallBitVec::iter`][1].
///
/// [1]: struct.SmallBitVec.html#method.iter
pub struct Iter<'a> {
    vec: &'a SmallBitVec,
    range: Range<u32>,
}

impl<'a> Iterator for Iter<'a> {
    type Item = bool;

    #[inline]
    fn next(&mut self) -> Option<bool> {
        self.range.next().map(|i| unsafe { self.vec.get_unchecked(i) })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.range.size_hint()
    }
}

impl<'a> DoubleEndedIterator for Iter<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<bool> {
        self.range.next_back().map(|i| unsafe { self.vec.get_unchecked(i) })
    }
}

impl<'a> ExactSizeIterator for Iter<'a> {}

#[cfg(test)]
mod tests {
    use super::*;

    macro_rules! v {
        ($elem:expr; $n:expr) => ({
            SmallBitVec::from_elem($n, $elem)
        });
        ($($x:expr),*) => ({
            [$($x),*].iter().cloned().collect::<SmallBitVec>()
        });
    }


    #[cfg(target_pointer_width = "32")]
    #[test]
    fn test_inline_capacity() {
        assert_eq!(inline_capacity(), 30);
    }

    #[cfg(target_pointer_width = "64")]
    #[test]
    fn test_inline_capacity() {
        assert_eq!(inline_capacity(), 62);
    }

    #[test]
    fn new() {
        let v = SmallBitVec::new();
        assert_eq!(v.len(), 0);
        assert_eq!(v.capacity(), inline_capacity());
        assert!(v.is_empty());
        assert!(v.is_inline());
    }

    #[test]
    fn with_capacity_inline() {
        for cap in 0..(inline_capacity() + 1) {
            let v = SmallBitVec::with_capacity(cap);
            assert_eq!(v.len(), 0);
            assert_eq!(v.capacity(), inline_capacity());
            assert!(v.is_inline());
        }
    }

    #[test]
    fn with_capacity_heap() {
        let cap = inline_capacity() + 1;
        let v = SmallBitVec::with_capacity(cap);
        assert_eq!(v.len(), 0);
        assert!(v.capacity() > inline_capacity());
        assert!(v.is_heap());
    }

    #[test]
    fn set_len_inline() {
        let mut v = SmallBitVec::new();
        for i in 0..(inline_capacity() + 1) {
            unsafe { v.set_len(i); }
            assert_eq!(v.len(), i);
        }
        for i in (0..(inline_capacity() + 1)).rev() {
            unsafe { v.set_len(i); }
            assert_eq!(v.len(), i);
        }
    }

    #[test]
    fn set_len_heap() {
        let mut v = SmallBitVec::with_capacity(500);
        unsafe { v.set_len(30); }
        assert_eq!(v.len(), 30);
    }

    #[test]
    fn push_many() {
        let mut v = SmallBitVec::new();
        for i in 0..500 {
            v.push(i % 3 == 0);
        }
        assert_eq!(v.len(), 500);

        for i in 0..500 {
            assert_eq!(v.get(i), (i % 3 == 0), "{}", i);
        }
    }

    #[test]
    #[should_panic]
    fn get_out_of_bounds() {
        let v = SmallBitVec::new();
        v.get(0);
    }

    #[test]
    #[should_panic]
    fn set_out_of_bounds() {
        let mut v = SmallBitVec::new();
        v.set(2, false);
    }

    #[test]
    fn all_false() {
        let mut v = SmallBitVec::new();
        assert!(v.all_false());
        for _ in 0..100 {
            v.push(false);
            assert!(v.all_false());

            let mut v2 = v.clone();
            v2.push(true);
            assert!(!v2.all_false());
        }
    }

    #[test]
    fn all_true() {
        let mut v = SmallBitVec::new();
        assert!(v.all_true());
        for _ in 0..100 {
            v.push(true);
            assert!(v.all_true());

            let mut v2 = v.clone();
            v2.push(false);
            assert!(!v2.all_true());
        }
    }

    #[test]
    fn iter() {
        let mut v = SmallBitVec::new();
        v.push(true);
        v.push(false);
        v.push(false);

        let mut i = v.iter();
        assert_eq!(i.next(), Some(true));
        assert_eq!(i.next(), Some(false));
        assert_eq!(i.next(), Some(false));
        assert_eq!(i.next(), None);
    }

    #[test]
    fn into_iter() {
        let mut v = SmallBitVec::new();
        v.push(true);
        v.push(false);
        v.push(false);

        let mut i = v.into_iter();
        assert_eq!(i.next(), Some(true));
        assert_eq!(i.next(), Some(false));
        assert_eq!(i.next(), Some(false));
        assert_eq!(i.next(), None);
    }

    #[test]
    fn iter_back() {
        let mut v = SmallBitVec::new();
        v.push(true);
        v.push(false);
        v.push(false);

        let mut i = v.iter();
        assert_eq!(i.next_back(), Some(false));
        assert_eq!(i.next_back(), Some(false));
        assert_eq!(i.next_back(), Some(true));
        assert_eq!(i.next(), None);
    }

    #[test]
    fn debug() {
        let mut v = SmallBitVec::new();
        v.push(true);
        v.push(false);
        v.push(false);

        assert_eq!(format!("{:?}", v), "[1, 0, 0]")
    }

    #[test]
    fn from_elem() {
        for len in 0..100 {
            let ones = SmallBitVec::from_elem(len, true);
            assert_eq!(ones.len(), len);
            assert!(ones.all_true());

            let zeros = SmallBitVec::from_elem(len, false);
            assert_eq!(zeros.len(), len);
            assert!(zeros.all_false());
        }
    }

    #[test]
    fn remove() {
        let mut v = SmallBitVec::new();
        v.push(false);
        v.push(true);
        v.push(false);
        v.push(false);
        v.push(true);

        v.remove(1);
        assert_eq!(format!("{:?}", v), "[0, 0, 0, 1]")
    }

    #[test]
    fn eq() {
        assert_eq!(v![], v![]);
        assert_eq!(v![true], v![true]);
        assert_eq!(v![false], v![false]);

        assert_ne!(v![], v![false]);
        assert_ne!(v![true], v![]);
        assert_ne!(v![true], v![false]);
        assert_ne!(v![false], v![true]);

        assert_eq!(v![true, false], v![true, false]);
        assert_eq!(v![true; 400], v![true; 400]);
        assert_eq!(v![false; 400], v![false; 400]);

        assert_ne!(v![true, false], v![true, true]);
        assert_ne!(v![true; 400], v![true; 401]);
        assert_ne!(v![false; 401], v![false; 400]);
    }
}
