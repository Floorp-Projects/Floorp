//! **arrayvec** provides the types `ArrayVec` and `ArrayString`: 
//! array-backed vector and string types, which store their contents inline.
//!
//! The **arrayvec** crate has the following cargo feature flags:
//!
//! - `std`
//!   - Optional, enabled by default
//!   - Requires Rust 1.6 *to disable*
//!   - Use libstd
//!
//! - `use_union`
//!   - Optional
//!   - Requires Rust nightly channel
//!   - Use the unstable feature untagged unions for the internal implementation,
//!     which has reduced space overhead
//!
//! - `use_generic_array`
//!   - Optional
//!   - Requires Rust stable channel
//!   - Depend on generic-array and allow using it just like a fixed
//!     size array for ArrayVec storage.
#![doc(html_root_url="https://docs.rs/arrayvec/0.3/")]
#![cfg_attr(not(feature="std"), no_std)]
extern crate odds;
extern crate nodrop;

#[cfg(feature = "use_generic_array")]
extern crate generic_array;

#[cfg(not(feature="std"))]
extern crate core as std;

use std::cmp;
use std::iter;
use std::mem;
use std::ptr;
use std::ops::{
    Deref,
    DerefMut,
};
use std::slice;

// extra traits
use std::borrow::{Borrow, BorrowMut};
use std::hash::{Hash, Hasher};
use std::fmt;

#[cfg(feature="std")]
use std::io;
#[cfg(feature="std")]
use std::error::Error;
#[cfg(feature="std")]
use std::any::Any; // core but unused

use nodrop::NoDrop;

mod array;
mod array_string;

pub use array::Array;
pub use odds::IndexRange as RangeArgument;
use array::Index;
pub use array_string::ArrayString;


unsafe fn new_array<A: Array>() -> A {
    // Note: Returning an uninitialized value here only works
    // if we can be sure the data is never used. The nullable pointer
    // inside enum optimization conflicts with this this for example,
    // so we need to be extra careful. See `NoDrop` enum.
    mem::uninitialized()
}

/// A vector with a fixed capacity.
///
/// The `ArrayVec` is a vector backed by a fixed size array. It keeps track of
/// the number of initialized elements.
///
/// The vector is a contiguous value that you can store directly on the stack
/// if needed.
///
/// It offers a simple API but also dereferences to a slice, so
/// that the full slice API is available.
///
/// ArrayVec can be converted into a by value iterator.
pub struct ArrayVec<A: Array> {
    xs: NoDrop<A>,
    len: A::Index,
}

impl<A: Array> Drop for ArrayVec<A> {
    fn drop(&mut self) {
        self.clear();

        // NoDrop inhibits array's drop
        // panic safety: NoDrop::drop will trigger on panic, so the inner
        // array will not drop even after panic.
    }
}

impl<A: Array> ArrayVec<A> {
    /// Create a new empty `ArrayVec`.
    ///
    /// Capacity is inferred from the type parameter.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::<[_; 16]>::new();
    /// array.push(1);
    /// array.push(2);
    /// assert_eq!(&array[..], &[1, 2]);
    /// assert_eq!(array.capacity(), 16);
    /// ```
    pub fn new() -> ArrayVec<A> {
        unsafe {
            ArrayVec { xs: NoDrop::new(new_array()), len: Index::from(0) }
        }
    }

    /// Return the number of elements in the `ArrayVec`.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::from([1, 2, 3]);
    /// array.pop();
    /// assert_eq!(array.len(), 2);
    /// ```
    #[inline]
    pub fn len(&self) -> usize { self.len.to_usize() }

    /// Return the capacity of the `ArrayVec`.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let array = ArrayVec::from([1, 2, 3]);
    /// assert_eq!(array.capacity(), 3);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize { A::capacity() }

    /// Return if the `ArrayVec` is completely filled.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::<[_; 1]>::new();
    /// assert!(!array.is_full());
    /// array.push(1);
    /// assert!(array.is_full());
    /// ```
    pub fn is_full(&self) -> bool { self.len() == self.capacity() }

    /// Push `element` to the end of the vector.
    ///
    /// Return `None` if the push succeeds, or and return `Some(` *element* `)`
    /// if the vector is full.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::<[_; 2]>::new();
    ///
    /// array.push(1);
    /// array.push(2);
    /// let overflow = array.push(3);
    ///
    /// assert_eq!(&array[..], &[1, 2]);
    /// assert_eq!(overflow, Some(3));
    /// ```
    pub fn push(&mut self, element: A::Item) -> Option<A::Item> {
        if self.len() < A::capacity() {
            let len = self.len();
            unsafe {
                ptr::write(self.get_unchecked_mut(len), element);
                self.set_len(len + 1);
            }
            None
        } else {
            Some(element)
        }
    }

    /// Insert `element` in position `index`.
    ///
    /// Shift up all elements after `index`. If any is pushed out, it is returned.
    ///
    /// Return `None` if no element is shifted out.
    ///
    /// `index` must be <= `self.len()` and < `self.capacity()`. Note that any
    /// out of bounds index insert results in the element being "shifted out"
    /// and returned directly.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::<[_; 2]>::new();
    ///
    /// assert_eq!(array.insert(0, "x"), None);
    /// assert_eq!(array.insert(0, "y"), None);
    /// assert_eq!(array.insert(0, "z"), Some("x"));
    /// assert_eq!(array.insert(1, "w"), Some("y"));
    /// assert_eq!(&array[..], &["z", "w"]);
    ///
    /// ```
    pub fn insert(&mut self, index: usize, element: A::Item) -> Option<A::Item> {
        if index > self.len() || index == self.capacity() {
            return Some(element);
        }
        let mut ret = None;
        if self.len() == self.capacity() {
            ret = self.pop();
        }
        let len = self.len();

        // follows is just like Vec<T>
        unsafe { // infallible
            // The spot to put the new value
            {
                let p = self.get_unchecked_mut(index) as *mut _;
                // Shift everything over to make space. (Duplicating the
                // `index`th element into two consecutive places.)
                ptr::copy(p, p.offset(1), len - index);
                // Write it in, overwriting the first copy of the `index`th
                // element.
                ptr::write(p, element);
            }
            self.set_len(len + 1);
        }
        ret
    }

    /// Remove the last element in the vector.
    ///
    /// Return `Some(` *element* `)` if the vector is non-empty, else `None`.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::<[_; 2]>::new();
    ///
    /// array.push(1);
    ///
    /// assert_eq!(array.pop(), Some(1));
    /// assert_eq!(array.pop(), None);
    /// ```
    pub fn pop(&mut self) -> Option<A::Item> {
        if self.len() == 0 {
            return None
        }
        unsafe {
            let new_len = self.len() - 1;
            self.set_len(new_len);
            Some(ptr::read(self.get_unchecked_mut(new_len)))
        }
    }

    /// Remove the element at `index` and swap the last element into its place.
    ///
    /// This operation is O(1).
    ///
    /// Return `Some(` *element* `)` if the index is in bounds, else `None`.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::from([1, 2, 3]);
    ///
    /// assert_eq!(array.swap_remove(0), Some(1));
    /// assert_eq!(&array[..], &[3, 2]);
    ///
    /// assert_eq!(array.swap_remove(10), None);
    /// ```
    pub fn swap_remove(&mut self, index: usize) -> Option<A::Item> {
        let len = self.len();
        if index >= len {
            return None
        }
        self.swap(index, len - 1);
        self.pop()
    }

    /// Remove the element at `index` and shift down the following elements.
    ///
    /// Return `Some(` *element* `)` if the index is in bounds, else `None`.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::from([1, 2, 3]);
    ///
    /// assert_eq!(array.remove(0), Some(1));
    /// assert_eq!(&array[..], &[2, 3]);
    ///
    /// assert_eq!(array.remove(10), None);
    /// ```
    pub fn remove(&mut self, index: usize) -> Option<A::Item> {
        if index >= self.len() {
            None
        } else {
            self.drain(index..index + 1).next()
        }
    }

    /// Remove all elements in the vector.
    pub fn clear(&mut self) {
        while let Some(_) = self.pop() { }
    }

    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&mut e)` returns false.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut array = ArrayVec::from([1, 2, 3, 4]);
    /// array.retain(|x| *x & 1 != 0 );
    /// assert_eq!(&array[..], &[1, 3]);
    /// ```
    pub fn retain<F>(&mut self, mut f: F)
        where F: FnMut(&mut A::Item) -> bool
    {
        let len = self.len();
        let mut del = 0;
        {
            let v = &mut **self;

            for i in 0..len {
                if !f(&mut v[i]) {
                    del += 1;
                } else if del > 0 {
                    v.swap(i - del, i);
                }
            }
        }
        if del > 0 {
            self.drain(len - del..);
        }
    }

    /// Set the vector's length without dropping or moving out elements
    ///
    /// May panic if `length` is greater than the capacity.
    ///
    /// This function is `unsafe` because it changes the notion of the
    /// number of “valid” elements in the vector. Use with care.
    #[inline]
    pub unsafe fn set_len(&mut self, length: usize) {
        debug_assert!(length <= self.capacity());
        self.len = Index::from(length);
    }


    /// Create a draining iterator that removes the specified range in the vector
    /// and yields the removed items from start to end. The element range is
    /// removed even if the iterator is not consumed until the end.
    ///
    /// Note: It is unspecified how many elements are removed from the vector,
    /// if the `Drain` value is leaked.
    ///
    /// **Panics** if the starting point is greater than the end point or if
    /// the end point is greater than the length of the vector.
    ///
    /// ```
    /// use arrayvec::ArrayVec;
    ///
    /// let mut v = ArrayVec::from([1, 2, 3]);
    /// let u: Vec<_> = v.drain(0..2).collect();
    /// assert_eq!(&v[..], &[3]);
    /// assert_eq!(&u[..], &[1, 2]);
    /// ```
    pub fn drain<R: RangeArgument>(&mut self, range: R) -> Drain<A> {
        // Memory safety
        //
        // When the Drain is first created, it shortens the length of
        // the source vector to make sure no uninitalized or moved-from elements
        // are accessible at all if the Drain's destructor never gets to run.
        //
        // Drain will ptr::read out the values to remove.
        // When finished, remaining tail of the vec is copied back to cover
        // the hole, and the vector length is restored to the new length.
        //
        let len = self.len();
        let start = range.start().unwrap_or(0);
        let end = range.end().unwrap_or(len);
        // bounds check happens here
        let range_slice: *const _ = &self[start..end];

        unsafe {
            // set self.vec length's to start, to be safe in case Drain is leaked
            self.set_len(start);
            Drain {
                tail_start: end,
                tail_len: len - end,
                iter: (*range_slice).iter(),
                vec: self as *mut _,
            }
        }
    }

    /// Return the inner fixed size array, if it is full to its capacity.
    ///
    /// Return an `Ok` value with the array if length equals capacity,
    /// return an `Err` with self otherwise.
    ///
    /// `Note:` This function may incur unproportionally large overhead
    /// to move the array out, its performance is not optimal.
    pub fn into_inner(self) -> Result<A, Self> {
        if self.len() < self.capacity() {
            Err(self)
        } else {
            unsafe {
                let array = ptr::read(&*self.xs);
                mem::forget(self);
                Ok(array)
            }
        }
    }

    /// Dispose of `self` without the overwriting that is needed in Drop.
    pub fn dispose(mut self) {
        self.clear();
        mem::forget(self);
    }

    /// Return a slice containing all elements of the vector.
    pub fn as_slice(&self) -> &[A::Item] {
        self
    }

    /// Return a mutable slice containing all elements of the vector.
    pub fn as_mut_slice(&mut self) -> &mut [A::Item] {
        self
    }
}

impl<A: Array> Deref for ArrayVec<A> {
    type Target = [A::Item];
    #[inline]
    fn deref(&self) -> &[A::Item] {
        unsafe {
            slice::from_raw_parts(self.xs.as_ptr(), self.len())
        }
    }
}

impl<A: Array> DerefMut for ArrayVec<A> {
    #[inline]
    fn deref_mut(&mut self) -> &mut [A::Item] {
        let len = self.len();
        unsafe {
            slice::from_raw_parts_mut(self.xs.as_mut_ptr(), len)
        }
    }
}

/// Create an `ArrayVec` from an array.
///
/// ```
/// use arrayvec::ArrayVec;
///
/// let mut array = ArrayVec::from([1, 2, 3]);
/// assert_eq!(array.len(), 3);
/// assert_eq!(array.capacity(), 3);
/// ```
impl<A: Array> From<A> for ArrayVec<A> {
    fn from(array: A) -> Self {
        ArrayVec { xs: NoDrop::new(array), len: Index::from(A::capacity()) }
    }
}


/// Iterate the `ArrayVec` with references to each element.
///
/// ```
/// use arrayvec::ArrayVec;
///
/// let array = ArrayVec::from([1, 2, 3]);
///
/// for elt in &array {
///     // ...
/// }
/// ```
impl<'a, A: Array> IntoIterator for &'a ArrayVec<A> {
    type Item = &'a A::Item;
    type IntoIter = slice::Iter<'a, A::Item>;
    fn into_iter(self) -> Self::IntoIter { self.iter() }
}

/// Iterate the `ArrayVec` with mutable references to each element.
///
/// ```
/// use arrayvec::ArrayVec;
///
/// let mut array = ArrayVec::from([1, 2, 3]);
///
/// for elt in &mut array {
///     // ...
/// }
/// ```
impl<'a, A: Array> IntoIterator for &'a mut ArrayVec<A> {
    type Item = &'a mut A::Item;
    type IntoIter = slice::IterMut<'a, A::Item>;
    fn into_iter(self) -> Self::IntoIter { self.iter_mut() }
}

/// Iterate the `ArrayVec` with each element by value.
///
/// The vector is consumed by this operation.
///
/// ```
/// use arrayvec::ArrayVec;
///
/// for elt in ArrayVec::from([1, 2, 3]) {
///     // ...
/// }
/// ```
impl<A: Array> IntoIterator for ArrayVec<A> {
    type Item = A::Item;
    type IntoIter = IntoIter<A>;
    fn into_iter(self) -> IntoIter<A> {
        IntoIter { index: Index::from(0), v: self, }
    }
}


/// By-value iterator for `ArrayVec`.
pub struct IntoIter<A: Array> {
    index: A::Index,
    v: ArrayVec<A>,
}

impl<A: Array> Iterator for IntoIter<A> {
    type Item = A::Item;

    #[inline]
    fn next(&mut self) -> Option<A::Item> {
        if self.index == self.v.len {
            None
        } else {
            unsafe {
                let index = self.index.to_usize();
                self.index = Index::from(index + 1);
                Some(ptr::read(self.v.get_unchecked_mut(index)))
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.v.len() - self.index.to_usize();
        (len, Some(len))
    }
}

impl<A: Array> DoubleEndedIterator for IntoIter<A> {
    #[inline]
    fn next_back(&mut self) -> Option<A::Item> {
        if self.index == self.v.len {
            None
        } else {
            unsafe {
                let new_len = self.v.len() - 1;
                self.v.set_len(new_len);
                Some(ptr::read(self.v.get_unchecked_mut(new_len)))
            }
        }
    }
}

impl<A: Array> ExactSizeIterator for IntoIter<A> { }

impl<A: Array> Drop for IntoIter<A> {
    fn drop(&mut self) {
        // panic safety: Set length to 0 before dropping elements.
        let index = self.index.to_usize();
        let len = self.v.len();
        unsafe {
            self.v.set_len(0);
            let elements = slice::from_raw_parts(self.v.get_unchecked_mut(index),
                                                 len - index);
            for elt in elements {
                ptr::read(elt);
            }
        }
    }
}

/// A draining iterator for `ArrayVec`.
pub struct Drain<'a, A> 
    where A: Array,
          A::Item: 'a,
{
    /// Index of tail to preserve
    tail_start: usize,
    /// Length of tail
    tail_len: usize,
    /// Current remaining range to remove
    iter: slice::Iter<'a, A::Item>,
    vec: *mut ArrayVec<A>,
}

unsafe impl<'a, A: Array + Sync> Sync for Drain<'a, A> {}
unsafe impl<'a, A: Array + Send> Send for Drain<'a, A> {}

impl<'a, A: Array> Iterator for Drain<'a, A>
    where A::Item: 'a,
{
    type Item = A::Item;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|elt|
            unsafe {
                ptr::read(elt as *const _)
            }
        )
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, A: Array> DoubleEndedIterator for Drain<'a, A>
    where A::Item: 'a,
{
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        self.iter.next_back().map(|elt|
            unsafe {
                ptr::read(elt as *const _)
            }
        )
    }
}

impl<'a, A: Array> ExactSizeIterator for Drain<'a, A> where A::Item: 'a {}

impl<'a, A: Array> Drop for Drain<'a, A> 
    where A::Item: 'a
{
    fn drop(&mut self) {
        // len is currently 0 so panicking while dropping will not cause a double drop.

        // exhaust self first
        while let Some(_) = self.next() { }

        if self.tail_len > 0 {
            unsafe {
                let source_vec = &mut *self.vec;
                // memmove back untouched tail, update to new length
                let start = source_vec.len();
                let tail = self.tail_start;
                let src = source_vec.as_ptr().offset(tail as isize);
                let dst = source_vec.as_mut_ptr().offset(start as isize);
                ptr::copy(src, dst, self.tail_len);
                source_vec.set_len(start + self.tail_len);
            }
        }
    }
}




/// Extend the `ArrayVec` with an iterator.
/// 
/// Does not extract more items than there is space for. No error
/// occurs if there are more iterator elements.
impl<A: Array> Extend<A::Item> for ArrayVec<A> {
    fn extend<T: IntoIterator<Item=A::Item>>(&mut self, iter: T) {
        let take = self.capacity() - self.len();
        for elt in iter.into_iter().take(take) {
            self.push(elt);
        }
    }
}

/// Create an `ArrayVec` from an iterator.
/// 
/// Does not extract more items than there is space for. No error
/// occurs if there are more iterator elements.
impl<A: Array> iter::FromIterator<A::Item> for ArrayVec<A> {
    fn from_iter<T: IntoIterator<Item=A::Item>>(iter: T) -> Self {
        let mut array = ArrayVec::new();
        array.extend(iter);
        array
    }
}

impl<A: Array> Clone for ArrayVec<A>
    where A::Item: Clone
{
    fn clone(&self) -> Self {
        self.iter().cloned().collect()
    }

    fn clone_from(&mut self, rhs: &Self) {
        // recursive case for the common prefix
        let prefix = cmp::min(self.len(), rhs.len());
        {
            let a = &mut self[..prefix];
            let b = &rhs[..prefix];
            for i in 0..prefix {
                a[i].clone_from(&b[i]);
            }
        }
        if prefix < self.len() {
            // rhs was shorter
            for _ in 0..self.len() - prefix {
                self.pop();
            }
        } else {
            for elt in &rhs[self.len()..] {
                self.push(elt.clone());
            }
        }
    }
}

impl<A: Array> Hash for ArrayVec<A>
    where A::Item: Hash
{
    fn hash<H: Hasher>(&self, state: &mut H) {
        Hash::hash(&**self, state)
    }
}

impl<A: Array> PartialEq for ArrayVec<A>
    where A::Item: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        **self == **other
    }
}

impl<A: Array> PartialEq<[A::Item]> for ArrayVec<A>
    where A::Item: PartialEq
{
    fn eq(&self, other: &[A::Item]) -> bool {
        **self == *other
    }
}

impl<A: Array> Eq for ArrayVec<A> where A::Item: Eq { }

impl<A: Array> Borrow<[A::Item]> for ArrayVec<A> {
    fn borrow(&self) -> &[A::Item] { self }
}

impl<A: Array> BorrowMut<[A::Item]> for ArrayVec<A> {
    fn borrow_mut(&mut self) -> &mut [A::Item] { self }
}

impl<A: Array> AsRef<[A::Item]> for ArrayVec<A> {
    fn as_ref(&self) -> &[A::Item] { self }
}

impl<A: Array> AsMut<[A::Item]> for ArrayVec<A> {
    fn as_mut(&mut self) -> &mut [A::Item] { self }
}

impl<A: Array> fmt::Debug for ArrayVec<A> where A::Item: fmt::Debug {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { (**self).fmt(f) }
}

impl<A: Array> Default for ArrayVec<A> {
    fn default() -> ArrayVec<A> {
        ArrayVec::new()
    }
}

impl<A: Array> PartialOrd for ArrayVec<A> where A::Item: PartialOrd {
    #[inline]
    fn partial_cmp(&self, other: &ArrayVec<A>) -> Option<cmp::Ordering> {
        (**self).partial_cmp(other)
    }

    #[inline]
    fn lt(&self, other: &Self) -> bool {
        (**self).lt(other)
    }

    #[inline]
    fn le(&self, other: &Self) -> bool {
        (**self).le(other)
    }

    #[inline]
    fn ge(&self, other: &Self) -> bool {
        (**self).ge(other)
    }

    #[inline]
    fn gt(&self, other: &Self) -> bool {
        (**self).gt(other)
    }
}

impl<A: Array> Ord for ArrayVec<A> where A::Item: Ord {
    fn cmp(&self, other: &ArrayVec<A>) -> cmp::Ordering {
        (**self).cmp(other)
    }
}

#[cfg(feature="std")]
/// `Write` appends written data to the end of the vector.
///
/// Requires `features="std"`.
impl<A: Array<Item=u8>> io::Write for ArrayVec<A> {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        unsafe {
            let len = self.len();
            let mut tail = slice::from_raw_parts_mut(self.get_unchecked_mut(len),
                                                     A::capacity() - len);
            let result = tail.write(data);
            if let Ok(written) = result {
                self.set_len(len + written);
            }
            result
        }
    }
    fn flush(&mut self) -> io::Result<()> { Ok(()) }
}

/// Error value indicating insufficient capacity
#[derive(Clone, Copy, Eq, Ord, PartialEq, PartialOrd)]
pub struct CapacityError<T = ()> {
    element: T,
}

impl<T> CapacityError<T> {
    fn new(element: T) -> CapacityError<T> {
        CapacityError {
            element: element,
        }
    }

    /// Extract the overflowing element
    pub fn element(self) -> T {
        self.element
    }

    /// Convert into a `CapacityError` that does not carry an element.
    pub fn simplify(self) -> CapacityError {
        CapacityError { element: () }
    }
}

const CAPERROR: &'static str = "insufficient capacity";

#[cfg(feature="std")]
/// Requires `features="std"`.
impl<T: Any> Error for CapacityError<T> {
    fn description(&self) -> &str {
        CAPERROR
    }
}

impl<T> fmt::Display for CapacityError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", CAPERROR)
    }
}

impl<T> fmt::Debug for CapacityError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}: {}", "CapacityError", CAPERROR)
    }
}
