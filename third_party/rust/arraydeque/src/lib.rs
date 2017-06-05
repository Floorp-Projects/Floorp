//! A circular buffer with fixed capacity.
//! Requires Rust 1.15+
//!
//! It can be stored directly on the stack if needed.
//!
//! This queue has `O(1)` amortized inserts and removals from both ends of the
//! container. It also has `O(1)` indexing like a vector. The contained elements
//! are not required to be copyable
//!
//! This crate is inspired by [**bluss/arrayvec**]
//! [**bluss/arrayvec**]: https://github.com/bluss/arrayvec
//!
//! # Feature Flags
//! The **arraydeque** crate has the following cargo feature flags:
//!
//! - `std`
//!   - Optional, enabled by default
//!   - Use libstd
//!
//!
//! - `use_union`
//!   - Optional
//!   - Requires Rust nightly channel
//!   - Use the unstable feature untagged unions for the internal implementation,
//!     which has reduced space overhead
//!
//!
//! - `use_generic_array`
//!   - Optional
//!   - Requires Rust stable channel
//!   - Depend on generic-array and allow using it just like a fixed
//!     size array for ArrayDeque storage.
//!
//!
//! # Usage
//!
//! First, add the following to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! arraydeque = "0.2"
//! ```
//!
//! Next, add this to your crate root:
//!
//! ```
//! extern crate arraydeque;
//! ```
//!
//! Currently arraydeque by default links to the standard library, but if you would
//! instead like to use arraydeque in a `#![no_std]` situation or crate you can
//! request this via:
//!
//! ```toml
//! [dependencies]
//! arraydeque = { version = "0.2", default-features = false }
//! ```
//!
//! # Capacity
//!
//! Note that the `capacity()` is always `backed_array.len() - 1`.
//! [Read more]
//!
//! [Read more]: https://en.wikipedia.org/wiki/Circular_buffer
//!
//! # Examples
//! ```
//! extern crate arraydeque;
//!
//! use arraydeque::ArrayDeque;
//!
//! fn main() {
//!     let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
//!     assert_eq!(vector.capacity(), 7);
//!     assert_eq!(vector.len(), 0);
//!
//!     vector.push_back(1);
//!     vector.push_back(2);
//!     assert_eq!(vector.len(), 2);
//!
//!     assert_eq!(vector.pop_front(), Some(1));
//!     assert_eq!(vector.pop_front(), Some(2));
//!     assert_eq!(vector.pop_front(), None);
//! }
//! ```
//!
//! # Insert & Remove
//! ```
//! use arraydeque::ArrayDeque;
//!
//! let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
//!
//! vector.push_back(11);
//! vector.push_back(13);
//! vector.insert(1, 12);
//! vector.remove(0);
//!
//! assert_eq!(vector[0], 12);
//! assert_eq!(vector[1], 13);
//! ```
//!
//! # Append & Extend
//! ```
//! use arraydeque::ArrayDeque;
//!
//! let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
//! let mut vector2: ArrayDeque<[_; 8]> = ArrayDeque::new();
//!
//! vector.extend(0..5);
//! vector2.extend(5..7);
//!
//! assert_eq!(format!("{:?}", vector), "[0, 1, 2, 3, 4]");
//! assert_eq!(format!("{:?}", vector2), "[5, 6]");
//!
//! vector.append(&mut vector2);
//!
//! assert_eq!(format!("{:?}", vector), "[0, 1, 2, 3, 4, 5, 6]");
//! assert_eq!(format!("{:?}", vector2), "[]");
//! ```
//!
//! # Iterator
//! ```
//! use arraydeque::ArrayDeque;
//!
//! let mut vector: ArrayDeque<[_; 8]> = ArrayDeque::new();
//!
//! vector.extend(0..5);
//!
//! let iters: Vec<_> = vector.into_iter().collect();
//! assert_eq!(iters, vec![0, 1, 2, 3, 4]);
//! ```
//!
//! # From Iterator
//! ```
//! use arraydeque::ArrayDeque;
//!
//! let vector: ArrayDeque<[_; 8]>;
//! let vector2: ArrayDeque<[_; 8]>;
//!
//! vector = vec![0, 1, 2, 3, 4].into_iter().collect();
//!
//! vector2 = (0..5).into_iter().collect();
//!
//! assert_eq!(vector, vector2);
//! ```
//!
//! # Generic Array
//! ```toml
//! [dependencies]
//! generic-array = "0.5.1"
//!
//! [dependencies.arraydeque]
//! version = "0.2"
//! features = ["use_generic_array"]
//! ```
//! ```
//! # #[cfg(feature = "use_generic_array")]
//! #[macro_use]
//! extern crate generic_array;
//! extern crate arraydeque;
//!
//! # #[cfg(feature = "use_generic_array")]
//! use generic_array::GenericArray;
//! # #[cfg(feature = "use_generic_array")]
//! use generic_array::typenum::U41;
//!
//! use arraydeque::ArrayDeque;
//!
//! # #[cfg(feature = "use_generic_array")]
//! fn main() {
//!     let mut vec: ArrayDeque<GenericArray<i32, U41>> = ArrayDeque::new();
//!
//!     assert_eq!(vec.len(), 0);
//!     assert_eq!(vec.capacity(), 40);
//!
//!     vec.extend(0..20);
//!
//!     assert_eq!(vec.len(), 20);
//!     assert_eq!(vec.into_iter().take(5).collect::<Vec<_>>(), vec![0, 1, 2, 3, 4]);
//! }
//!
//! # #[cfg(not(feature = "use_generic_array"))]
//! # fn main() {}
//! ```

#![cfg_attr(not(any(feature="std", test)), no_std)]

extern crate odds;
extern crate nodrop;

#[cfg(feature = "use_generic_array")]
extern crate generic_array;

// #![cfg_attr(not(or(feature="std", test), no_std))]
#[cfg(not(any(feature="std", test)))]
extern crate core as std;

use std::mem;
use std::cmp;
use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use std::fmt;
use std::ptr;
use std::slice;
use std::iter;
use std::ops::Index;
use std::ops::IndexMut;

pub use odds::IndexRange as RangeArgument;
use nodrop::NoDrop;

pub mod array;

pub use array::Array;
use array::Index as ArrayIndex;

unsafe fn new_array<A: Array>() -> A {
    // Note: Returning an uninitialized value here only works
    // if we can be sure the data is never used. The nullable pointer
    // inside enum optimization conflicts with this this for example,
    // so we need to be extra careful. See `NoDrop` enum.
    mem::uninitialized()
}

/// A fixed capacity ring buffer.
///
/// It can be stored directly on the stack if needed.
///
/// The "default" usage of this type as a queue is to use `push_back` to add to
/// the queue, and `pop_front` to remove from the queue. `extend` and `append`
/// push onto the back in this manner, and iterating over `ArrayDeque` goes front
/// to back.
///
/// # Capacity
///
/// Note that the `capacity()` is always `backed_array.len() - 1`.
/// [Read more]
///
/// [Read more]: https://en.wikipedia.org/wiki/Circular_buffer
pub struct ArrayDeque<A: Array> {
    xs: NoDrop<A>,
    head: A::Index,
    tail: A::Index,
}

impl<A: Array> Clone for ArrayDeque<A>
    where A::Item: Clone
{
    fn clone(&self) -> ArrayDeque<A> {
        self.iter().cloned().collect()
    }
}

impl<A: Array> Drop for ArrayDeque<A> {
    fn drop(&mut self) {
        self.clear();

        // NoDrop inhibits array's drop
        // panic safety: NoDrop::drop will trigger on panic, so the inner
        // array will not drop even after panic.
    }
}

impl<A: Array> Default for ArrayDeque<A> {
    #[inline]
    fn default() -> ArrayDeque<A> {
        ArrayDeque::new()
    }
}

impl<A: Array> ArrayDeque<A> {
    #[inline]
    fn wrap_add(index: usize, addend: usize) -> usize {
        wrap_add(index, addend, A::capacity())
    }

    #[inline]
    fn wrap_sub(index: usize, subtrahend: usize) -> usize {
        wrap_sub(index, subtrahend, A::capacity())
    }

    #[inline]
    fn ptr(&self) -> *const A::Item {
        self.xs.as_ptr()
    }

    #[inline]
    fn ptr_mut(&mut self) -> *mut A::Item {
        self.xs.as_mut_ptr()
    }

    #[inline]
    fn is_contiguous(&self) -> bool {
        self.tail() <= self.head()
    }

    #[inline]
    fn is_full(&self) -> bool {
        A::capacity() - self.len() == 1
    }

    #[inline]
    fn head(&self) -> usize {
        self.head.to_usize()
    }

    #[inline]
    fn tail(&self) -> usize {
        self.tail.to_usize()
    }

    #[inline]
    unsafe fn set_head(&mut self, head: usize) {
        debug_assert!(head <= self.capacity());
        self.head = ArrayIndex::from(head);
    }

    #[inline]
    unsafe fn set_tail(&mut self, tail: usize) {
        debug_assert!(tail <= self.capacity());
        self.tail = ArrayIndex::from(tail);
    }

    /// Copies a contiguous block of memory len long from src to dst
    #[inline]
    unsafe fn copy(&mut self, dst: usize, src: usize, len: usize) {
        debug_assert!(dst + len <= A::capacity(),
                      "cpy dst={} src={} len={} cap={}",
                      dst,
                      src,
                      len,
                      A::capacity());
        debug_assert!(src + len <= A::capacity(),
                      "cpy dst={} src={} len={} cap={}",
                      dst,
                      src,
                      len,
                      A::capacity());
        ptr::copy(self.ptr_mut().offset(src as isize),
                  self.ptr_mut().offset(dst as isize),
                  len);
    }

    /// Copies a potentially wrapping block of memory len long from src to dest.
    /// (abs(dst - src) + len) must be no larger than cap() (There must be at
    /// most one continuous overlapping region between src and dest).
    unsafe fn wrap_copy(&mut self, dst: usize, src: usize, len: usize) {
        #[allow(dead_code)]
        fn diff(a: usize, b: usize) -> usize {
            if a <= b { b - a } else { a - b }
        }
        debug_assert!(cmp::min(diff(dst, src), A::capacity() - diff(dst, src)) + len <=
                      A::capacity(),
                      "wrc dst={} src={} len={} cap={}",
                      dst,
                      src,
                      len,
                      A::capacity());

        if src == dst || len == 0 {
            return;
        }

        let dst_after_src = Self::wrap_sub(dst, src) < len;

        let src_pre_wrap_len = A::capacity() - src;
        let dst_pre_wrap_len = A::capacity() - dst;
        let src_wraps = src_pre_wrap_len < len;
        let dst_wraps = dst_pre_wrap_len < len;

        match (dst_after_src, src_wraps, dst_wraps) {
            (_, false, false) => {
                // src doesn't wrap, dst doesn't wrap
                //
                //        S . . .
                // 1 [_ _ A A B B C C _]
                // 2 [_ _ A A A A B B _]
                //            D . . .
                //
                self.copy(dst, src, len);
            }
            (false, false, true) => {
                // dst before src, src doesn't wrap, dst wraps
                //
                //    S . . .
                // 1 [A A B B _ _ _ C C]
                // 2 [A A B B _ _ _ A A]
                // 3 [B B B B _ _ _ A A]
                //    . .           D .
                //
                self.copy(dst, src, dst_pre_wrap_len);
                self.copy(0, src + dst_pre_wrap_len, len - dst_pre_wrap_len);
            }
            (true, false, true) => {
                // src before dst, src doesn't wrap, dst wraps
                //
                //              S . . .
                // 1 [C C _ _ _ A A B B]
                // 2 [B B _ _ _ A A B B]
                // 3 [B B _ _ _ A A A A]
                //    . .           D .
                //
                self.copy(0, src + dst_pre_wrap_len, len - dst_pre_wrap_len);
                self.copy(dst, src, dst_pre_wrap_len);
            }
            (false, true, false) => {
                // dst before src, src wraps, dst doesn't wrap
                //
                //    . .           S .
                // 1 [C C _ _ _ A A B B]
                // 2 [C C _ _ _ B B B B]
                // 3 [C C _ _ _ B B C C]
                //              D . . .
                //
                self.copy(dst, src, src_pre_wrap_len);
                self.copy(dst + src_pre_wrap_len, 0, len - src_pre_wrap_len);
            }
            (true, true, false) => {
                // src before dst, src wraps, dst doesn't wrap
                //
                //    . .           S .
                // 1 [A A B B _ _ _ C C]
                // 2 [A A A A _ _ _ C C]
                // 3 [C C A A _ _ _ C C]
                //    D . . .
                //
                self.copy(dst + src_pre_wrap_len, 0, len - src_pre_wrap_len);
                self.copy(dst, src, src_pre_wrap_len);
            }
            (false, true, true) => {
                // dst before src, src wraps, dst wraps
                //
                //    . . .         S .
                // 1 [A B C D _ E F G H]
                // 2 [A B C D _ E G H H]
                // 3 [A B C D _ E G H A]
                // 4 [B C C D _ E G H A]
                //    . .         D . .
                //
                debug_assert!(dst_pre_wrap_len > src_pre_wrap_len);
                let delta = dst_pre_wrap_len - src_pre_wrap_len;
                self.copy(dst, src, src_pre_wrap_len);
                self.copy(dst + src_pre_wrap_len, 0, delta);
                self.copy(0, delta, len - dst_pre_wrap_len);
            }
            (true, true, true) => {
                // src before dst, src wraps, dst wraps
                //
                //    . .         S . .
                // 1 [A B C D _ E F G H]
                // 2 [A A B D _ E F G H]
                // 3 [H A B D _ E F G H]
                // 4 [H A B D _ E F F G]
                //    . . .         D .
                //
                debug_assert!(src_pre_wrap_len > dst_pre_wrap_len);
                let delta = src_pre_wrap_len - dst_pre_wrap_len;
                self.copy(delta, 0, len - src_pre_wrap_len);
                self.copy(0, A::capacity() - delta, delta);
                self.copy(dst, src, dst_pre_wrap_len);
            }
        }
    }

    #[inline]
    unsafe fn buffer_as_slice(&self) -> &[A::Item] {
        slice::from_raw_parts(self.ptr(), A::capacity())
    }

    #[inline]
    unsafe fn buffer_as_mut_slice(&mut self) -> &mut [A::Item] {
        slice::from_raw_parts_mut(self.ptr_mut(), A::capacity())
    }

    #[inline]
    unsafe fn buffer_read(&mut self, offset: usize) -> A::Item {
        ptr::read(self.ptr().offset(offset as isize))
    }

    #[inline]
    unsafe fn buffer_write(&mut self, offset: usize, element: A::Item) {
        ptr::write(self.ptr_mut().offset(offset as isize), element);
    }
}

impl<A: Array> ArrayDeque<A> {
    /// Creates an empty `ArrayDeque`.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let vector: ArrayDeque<[usize; 3]> = ArrayDeque::new();
    /// ```
    #[inline]
    pub fn new() -> ArrayDeque<A> {
        unsafe {
            ArrayDeque {
                xs: NoDrop::new(::new_array()),
                head: ArrayIndex::from(0),
                tail: ArrayIndex::from(0),
            }
        }
    }

    /// Retrieves an element in the `ArrayDeque` by index.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(3);
    /// buf.push_back(4);
    /// buf.push_back(5);
    /// assert_eq!(buf.get(1), Some(&4));
    /// ```
    #[inline]
    pub fn get(&self, index: usize) -> Option<&A::Item> {
        if index < self.len() {
            let idx = Self::wrap_add(self.tail(), index);
            unsafe { Some(&*self.ptr().offset(idx as isize)) }
        } else {
            None
        }
    }

    /// Retrieves an element in the `ArrayDeque` mutably by index.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(3);
    /// buf.push_back(4);
    /// buf.push_back(5);
    /// if let Some(elem) = buf.get_mut(1) {
    ///     *elem = 7;
    /// }
    ///
    /// assert_eq!(buf[1], 7);
    /// ```
    #[inline]
    pub fn get_mut(&mut self, index: usize) -> Option<&mut A::Item> {
        if index < self.len() {
            let idx = Self::wrap_add(self.tail(), index);
            unsafe { Some(&mut *self.ptr_mut().offset(idx as isize)) }
        } else {
            None
        }
    }

    /// Swaps elements at indices `i` and `j`.
    ///
    /// `i` and `j` may be equal.
    ///
    /// Fails if there is no element with either index.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(3);
    /// buf.push_back(4);
    /// buf.push_back(5);
    /// buf.swap(0, 2);
    /// assert_eq!(buf[0], 5);
    /// assert_eq!(buf[2], 3);
    /// ```
    #[inline]
    pub fn swap(&mut self, i: usize, j: usize) {
        assert!(i < self.len());
        assert!(j < self.len());
        let ri = Self::wrap_add(self.tail(), i);
        let rj = Self::wrap_add(self.tail(), j);
        unsafe {
            ptr::swap(self.ptr_mut().offset(ri as isize),
                      self.ptr_mut().offset(rj as isize))
        }
    }

    /// Return the capacity of the `ArrayDeque`.
    ///
    /// # Capacity
    ///
    /// Note that the `capacity()` is always `backed_array.len() - 1`.
    /// [Read more]
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[usize; 4]> = ArrayDeque::new();
    /// assert_eq!(buf.capacity(), 3);
    /// ```
    ///
    /// [Read more]: https://en.wikipedia.org/wiki/Circular_buffer
    #[inline]
    pub fn capacity(&self) -> usize {
        A::capacity() - 1
    }

    /// Returns a front-to-back iterator.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(5);
    /// buf.push_back(3);
    /// buf.push_back(4);
    /// let b: &[_] = &[&5, &3, &4];
    /// let c: Vec<&i32> = buf.iter().collect();
    /// assert_eq!(&c[..], b);
    /// ```
    #[inline]
    pub fn iter(&self) -> Iter<A::Item> {
        Iter {
            head: self.head(),
            tail: self.tail(),
            ring: unsafe { self.buffer_as_slice() },
        }
    }

    /// Returns a front-to-back iterator that returns mutable references.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(5);
    /// buf.push_back(3);
    /// buf.push_back(4);
    /// for num in buf.iter_mut() {
    ///     *num = *num - 2;
    /// }
    /// let b: &[_] = &[&mut 3, &mut 1, &mut 2];
    /// assert_eq!(&buf.iter_mut().collect::<Vec<&mut i32>>()[..], b);
    /// ```
    #[inline]
    pub fn iter_mut(&mut self) -> IterMut<A::Item> {
        IterMut {
            head: self.head(),
            tail: self.tail(),
            ring: unsafe { self.buffer_as_mut_slice() },
        }
    }

    /// Returns a pair of slices which contain, in order, the contents of the
    /// `ArrayDeque`.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut vector: ArrayDeque<[_; 6]> = ArrayDeque::new();
    ///
    /// vector.push_back(0);
    /// vector.push_back(1);
    /// vector.push_back(2);
    ///
    /// assert_eq!(vector.as_slices(), (&[0, 1, 2][..], &[][..]));
    ///
    /// vector.push_front(10);
    /// vector.push_front(9);
    ///
    /// assert_eq!(vector.as_slices(), (&[9, 10][..], &[0, 1, 2][..]));
    /// ```
    #[inline]
    pub fn as_slices(&self) -> (&[A::Item], &[A::Item]) {
        unsafe {
            let (first, second) = (*(self as *const Self as *mut Self)).as_mut_slices();
            (first, second)
        }
    }

    /// Returns a pair of slices which contain, in order, the contents of the
    /// `ArrayDeque`.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut vector: ArrayDeque<[_; 5]> = ArrayDeque::new();
    ///
    /// vector.push_back(0);
    /// vector.push_back(1);
    ///
    /// vector.push_front(10);
    /// vector.push_front(9);
    ///
    /// vector.as_mut_slices().0[0] = 42;
    /// vector.as_mut_slices().1[0] = 24;
    /// assert_eq!(vector.as_slices(), (&[42, 10][..], &[24, 1][..]));
    /// ```
    #[inline]
    pub fn as_mut_slices(&mut self) -> (&mut [A::Item], &mut [A::Item]) {
        unsafe {
            let contiguous = self.is_contiguous();
            let head = self.head();
            let tail = self.tail();
            let buf = self.buffer_as_mut_slice();

            if contiguous {
                let (empty, buf) = buf.split_at_mut(0);
                (&mut buf[tail..head], empty)
            } else {
                let (mid, right) = buf.split_at_mut(tail);
                let (left, _) = mid.split_at_mut(head);

                (right, left)
            }
        }
    }

    /// Returns the number of elements in the `ArrayDeque`.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut v: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// assert_eq!(v.len(), 0);
    /// v.push_back(1);
    /// assert_eq!(v.len(), 1);
    /// ```
    #[inline]
    pub fn len(&self) -> usize {
        count(self.tail(), self.head(), A::capacity())
    }

    /// Returns true if the buffer contains no elements
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut v: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// assert!(v.is_empty());
    /// v.push_front(1);
    /// assert!(!v.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.head() == self.tail()
    }

    /// Create a draining iterator that removes the specified range in the
    /// `ArrayDeque` and yields the removed items.
    ///
    /// Note 1: The element range is removed even if the iterator is not
    /// consumed until the end.
    ///
    /// Note 2: It is unspecified how many elements are removed from the deque,
    /// if the `Drain` value is not dropped, but the borrow it holds expires
    /// (eg. due to mem::forget).
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the vector.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut v: ArrayDeque<[_; 4]> = vec![1, 2, 3].into_iter().collect();
    /// assert_eq!(vec![3].into_iter().collect::<ArrayDeque<[_; 4]>>(), v.drain(2..).collect());
    /// assert_eq!(vec![1, 2].into_iter().collect::<ArrayDeque<[_; 4]>>(), v);
    ///
    /// // A full range clears all contents
    /// v.drain(..);
    /// assert!(v.is_empty());
    /// ```
    pub fn drain<R>(&mut self, range: R) -> Drain<A>
        where R: RangeArgument<usize>
    {
        let len = self.len();
        let start = range.start().unwrap_or(0);
        let end = range.end().unwrap_or(len);
        assert!(start <= end, "drain lower bound was too large");
        assert!(end <= len, "drain upper bound was too large");

        let drain_tail = Self::wrap_add(self.tail(), start);
        let drain_head = Self::wrap_add(self.tail(), end);
        let head = self.head();

        unsafe { self.set_head(drain_tail) }

        Drain {
            deque: self as *mut _,
            after_tail: drain_head,
            after_head: head,
            iter: Iter {
                tail: drain_tail,
                head: drain_head,
                ring: unsafe { self.buffer_as_mut_slice() },
            },
        }
    }

    /// Clears the buffer, removing all values.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut v: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// v.push_back(1);
    /// v.clear();
    /// assert!(v.is_empty());
    /// ```
    #[inline]
    pub fn clear(&mut self) {
        self.drain(..);
    }

    /// Returns `true` if the `ArrayDeque` contains an element equal to the
    /// given value.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut vector: ArrayDeque<[_; 3]> = ArrayDeque::new();
    ///
    /// vector.push_back(0);
    /// vector.push_back(1);
    ///
    /// assert_eq!(vector.contains(&1), true);
    /// assert_eq!(vector.contains(&10), false);
    /// ```
    pub fn contains(&self, x: &A::Item) -> bool
        where A::Item: PartialEq<A::Item>
    {
        let (a, b) = self.as_slices();
        a.contains(x) || b.contains(x)
    }

    /// Provides a reference to the front element, or `None` if the sequence is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// assert_eq!(d.front(), None);
    ///
    /// d.push_back(1);
    /// d.push_back(2);
    /// assert_eq!(d.front(), Some(&1));
    /// ```
    pub fn front(&self) -> Option<&A::Item> {
        if !self.is_empty() {
            Some(&self[0])
        } else {
            None
        }
    }

    /// Provides a mutable reference to the front element, or `None` if the
    /// sequence is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// assert_eq!(d.front_mut(), None);
    ///
    /// d.push_back(1);
    /// d.push_back(2);
    /// match d.front_mut() {
    ///     Some(x) => *x = 9,
    ///     None => (),
    /// }
    /// assert_eq!(d.front(), Some(&9));
    /// ```
    pub fn front_mut(&mut self) -> Option<&mut A::Item> {
        if !self.is_empty() {
            Some(&mut self[0])
        } else {
            None
        }
    }

    /// Provides a reference to the back element, or `None` if the sequence is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// assert_eq!(d.back(), None);
    ///
    /// d.push_back(1);
    /// d.push_back(2);
    /// assert_eq!(d.back(), Some(&2));
    /// ```
    pub fn back(&self) -> Option<&A::Item> {
        if !self.is_empty() {
            Some(&self[self.len() - 1])
        } else {
            None
        }
    }

    /// Provides a mutable reference to the back element, or `None` if the
    /// sequence is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// assert_eq!(d.back(), None);
    ///
    /// d.push_back(1);
    /// d.push_back(2);
    /// match d.back_mut() {
    ///     Some(x) => *x = 9,
    ///     None => (),
    /// }
    /// assert_eq!(d.back(), Some(&9));
    /// ```
    pub fn back_mut(&mut self) -> Option<&mut A::Item> {
        let len = self.len();
        if !self.is_empty() {
            Some(&mut self[len - 1])
        } else {
            None
        }
    }

    /// Removes the first element and returns it, or `None` if the sequence is
    /// empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// d.push_back(1);
    /// d.push_back(2);
    ///
    /// assert_eq!(d.pop_front(), Some(1));
    /// assert_eq!(d.pop_front(), Some(2));
    /// assert_eq!(d.pop_front(), None);
    /// ```
    pub fn pop_front(&mut self) -> Option<A::Item> {
        if self.is_empty() {
            return None;
        }
        unsafe {
            let tail = self.tail();
            self.set_tail(Self::wrap_add(tail, 1));
            Some(self.buffer_read(tail))
        }
    }

    /// Inserts an element first in the sequence.
    ///
    /// Return `None` if the push succeeds, or and return `Some(` *element* `)`
    /// if the vector is full.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut d: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// d.push_front(1);
    /// d.push_front(2);
    /// let overflow = d.push_front(3);
    ///
    /// assert_eq!(d.front(), Some(&2));
    /// assert_eq!(overflow, Some(3));
    /// ```
    pub fn push_front(&mut self, element: A::Item) -> Option<A::Item> {
        if !self.is_full() {
            unsafe {
                let new_tail = Self::wrap_sub(self.tail(), 1);
                self.set_tail(new_tail);
                self.buffer_write(new_tail, element);
            }
            None
        } else {
            Some(element)
        }
    }

    /// Appends an element to the back of a buffer
    ///
    /// Return `None` if the push succeeds, or and return `Some(` *element* `)`
    /// if the vector is full.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// buf.push_back(1);
    /// buf.push_back(3);
    /// let overflow = buf.push_back(5);
    ///
    /// assert_eq!(3, *buf.back().unwrap());
    /// assert_eq!(overflow, Some(5));
    /// ```
    pub fn push_back(&mut self, element: A::Item) -> Option<A::Item> {
        if !self.is_full() {
            unsafe {
                let head = self.head();
                self.set_head(Self::wrap_add(head, 1));
                self.buffer_write(head, element);
            }
            None
        } else {
            Some(element)
        }
    }

    /// Removes the last element from a buffer and returns it, or `None` if
    /// it is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 3]> = ArrayDeque::new();
    /// assert_eq!(buf.pop_back(), None);
    /// buf.push_back(1);
    /// buf.push_back(3);
    /// assert_eq!(buf.pop_back(), Some(3));
    /// ```
    pub fn pop_back(&mut self) -> Option<A::Item> {
        if self.is_empty() {
            return None;
        }
        unsafe {
            let new_head = Self::wrap_sub(self.head(), 1);
            self.set_head(new_head);
            Some(self.buffer_read(new_head))
        }
    }

    /// Removes an element from anywhere in the `ArrayDeque` and returns it, replacing it with the
    /// last element.
    ///
    /// This does not preserve ordering, but is O(1).
    ///
    /// Returns `None` if `index` is out of bounds.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// assert_eq!(buf.swap_remove_back(0), None);
    /// buf.push_back(1);
    /// buf.push_back(2);
    /// buf.push_back(3);
    ///
    /// assert_eq!(buf.swap_remove_back(0), Some(1));
    /// assert_eq!(buf.len(), 2);
    /// assert_eq!(buf[0], 3);
    /// assert_eq!(buf[1], 2);
    /// ```
    pub fn swap_remove_back(&mut self, index: usize) -> Option<A::Item> {
        let length = self.len();
        if length > 0 && index < length - 1 {
            self.swap(index, length - 1);
        } else if index >= length {
            return None;
        }
        self.pop_back()
    }

    /// Removes an element from anywhere in the `ArrayDeque` and returns it,
    /// replacing it with the first element.
    ///
    /// This does not preserve ordering, but is O(1).
    ///
    /// Returns `None` if `index` is out of bounds.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// assert_eq!(buf.swap_remove_front(0), None);
    /// buf.push_back(1);
    /// buf.push_back(2);
    /// buf.push_back(3);
    ///
    /// assert_eq!(buf.swap_remove_front(2), Some(3));
    /// assert_eq!(buf.len(), 2);
    /// assert_eq!(buf[0], 2);
    /// assert_eq!(buf[1], 1);
    /// ```
    pub fn swap_remove_front(&mut self, index: usize) -> Option<A::Item> {
        let length = self.len();
        if length > 0 && index < length && index != 0 {
            self.swap(index, 0);
        } else if index >= length {
            return None;
        }
        self.pop_front()
    }

    /// Inserts an element at `index` within the `ArrayDeque`. Whichever
    /// end is closer to the insertion point will be moved to make room,
    /// and all the affected elements will be moved to new positions.
    ///
    /// Return `None` if the push succeeds, or and return `Some(` *element* `)`
    /// if the vector is full.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Panics
    ///
    /// Panics if `index` is greater than `ArrayDeque`'s length
    ///
    /// # Examples
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(10);
    /// buf.push_back(12);
    /// buf.insert(1, 11);
    /// let overflow = buf.insert(0, 9);
    ///
    /// assert_eq!(Some(&11), buf.get(1));
    /// assert_eq!(overflow, Some(9));
    /// ```
    pub fn insert(&mut self, index: usize, element: A::Item) -> Option<A::Item> {
        assert!(index <= self.len(), "index out of bounds");
        if self.is_full() {
            return Some(element);
        }

        // Move the least number of elements in the ring buffer and insert
        // the given object
        //
        // At most len/2 - 1 elements will be moved. O(min(n, n-i))
        //
        // There are three main cases:
        //  Elements are contiguous
        //      - special case when tail is 0
        //  Elements are discontiguous and the insert is in the tail section
        //  Elements are discontiguous and the insert is in the head section
        //
        // For each of those there are two more cases:
        //  Insert is closer to tail
        //  Insert is closer to head
        //
        // Key: H - self.head
        //      T - self.tail
        //      o - Valid element
        //      I - Insertion element
        //      A - The element that should be after the insertion point
        //      M - Indicates element was moved

        let idx = Self::wrap_add(self.tail(), index);

        let distance_to_tail = index;
        let distance_to_head = self.len() - index;

        let contiguous = self.is_contiguous();

        match (contiguous, distance_to_tail <= distance_to_head, idx >= self.tail()) {
            (true, true, _) if index == 0 => {
                // push_front
                //
                //       T
                //       I             H
                //      [A o o o o o o . . . . . . . . .]
                //
                //                       H         T
                //      [A o o o o o o o . . . . . I]
                //

                let new_tail = Self::wrap_sub(self.tail(), 1);
                unsafe { self.set_tail(new_tail) }
            }
            (true, true, _) => {
                unsafe {
                    // contiguous, insert closer to tail:
                    //
                    //             T   I         H
                    //      [. . . o o A o o o o . . . . . .]
                    //
                    //           T               H
                    //      [. . o o I A o o o o . . . . . .]
                    //           M M
                    //
                    // contiguous, insert closer to tail and tail is 0:
                    //
                    //
                    //       T   I         H
                    //      [o o A o o o o . . . . . . . . .]
                    //
                    //                       H             T
                    //      [o I A o o o o o . . . . . . . o]
                    //       M                             M

                    let tail = self.tail();
                    let new_tail = Self::wrap_sub(self.tail(), 1);

                    self.copy(new_tail, tail, 1);
                    // Already moved the tail, so we only copy `index - 1` elements.
                    self.copy(tail, tail + 1, index - 1);

                    self.set_tail(new_tail);
                }
            }
            (true, false, _) => {
                unsafe {
                    //  contiguous, insert closer to head:
                    //
                    //             T       I     H
                    //      [. . . o o o o A o o . . . . . .]
                    //
                    //             T               H
                    //      [. . . o o o o I A o o . . . . .]
                    //                       M M M

                    let head = self.head();
                    self.copy(idx + 1, idx, head - idx);
                    let new_head = Self::wrap_add(self.head(), 1);
                    self.set_head(new_head);
                }
            }
            (false, true, true) => {
                unsafe {
                    // discontiguous, insert closer to tail, tail section:
                    //
                    //                   H         T   I
                    //      [o o o o o o . . . . . o o A o o]
                    //
                    //                   H       T
                    //      [o o o o o o . . . . o o I A o o]
                    //                           M M

                    let tail = self.tail();
                    self.copy(tail - 1, tail, index);
                    self.set_tail(tail - 1);
                }
            }
            (false, false, true) => {
                unsafe {
                    // discontiguous, insert closer to head, tail section:
                    //
                    //           H             T         I
                    //      [o o . . . . . . . o o o o o A o]
                    //
                    //             H           T
                    //      [o o o . . . . . . o o o o o I A]
                    //       M M M                         M

                    // copy elements up to new head
                    let head = self.head();
                    self.copy(1, 0, head);

                    // copy last element into empty spot at bottom of buffer
                    self.copy(0, A::capacity() - 1, 1);

                    // move elements from idx to end forward not including ^ element
                    self.copy(idx + 1, idx, A::capacity() - 1 - idx);

                    self.set_head(head + 1);
                }
            }
            (false, true, false) if idx == 0 => {
                unsafe {
                    // discontiguous, insert is closer to tail, head section,
                    // and is at index zero in the internal buffer:
                    //
                    //       I                   H     T
                    //      [A o o o o o o o o o . . . o o o]
                    //
                    //                           H   T
                    //      [A o o o o o o o o o . . o o o I]
                    //                               M M M

                    // copy elements up to new tail
                    let tail = self.tail();
                    self.copy(tail - 1, tail, A::capacity() - tail);

                    // copy last element into empty spot at bottom of buffer
                    self.copy(A::capacity() - 1, 0, 1);

                    self.set_tail(tail - 1);
                }
            }
            (false, true, false) => {
                unsafe {
                    // discontiguous, insert closer to tail, head section:
                    //
                    //             I             H     T
                    //      [o o o A o o o o o o . . . o o o]
                    //
                    //                           H   T
                    //      [o o I A o o o o o o . . o o o o]
                    //       M M                     M M M M

                    let tail = self.tail();
                    // copy elements up to new tail
                    self.copy(tail - 1, tail, A::capacity() - tail);

                    // copy last element into empty spot at bottom of buffer
                    self.copy(A::capacity() - 1, 0, 1);

                    // move elements from idx-1 to end forward not including ^ element
                    self.copy(0, 1, idx - 1);

                    self.set_tail(tail - 1);
                }
            }
            (false, false, false) => {
                unsafe {
                    // discontiguous, insert closer to head, head section:
                    //
                    //               I     H           T
                    //      [o o o o A o o . . . . . . o o o]
                    //
                    //                     H           T
                    //      [o o o o I A o o . . . . . o o o]
                    //                 M M M

                    let head = self.head();
                    self.copy(idx + 1, idx, head - idx);
                    self.set_head(head + 1);
                }
            }
        }

        // tail might've been changed so we need to recalculate
        let new_idx = Self::wrap_add(self.tail(), index);
        unsafe {
            self.buffer_write(new_idx, element);
        }

        None
    }

    /// Removes and returns the element at `index` from the `ArrayDeque`.
    /// Whichever end is closer to the removal point will be moved to make
    /// room, and all the affected elements will be moved to new positions.
    /// Returns `None` if `index` is out of bounds.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = ArrayDeque::new();
    /// buf.push_back(1);
    /// buf.push_back(2);
    /// buf.push_back(3);
    ///
    /// assert_eq!(buf.remove(1), Some(2));
    /// assert_eq!(buf.get(1), Some(&3));
    /// ```
    pub fn remove(&mut self, index: usize) -> Option<A::Item> {
        if self.is_empty() || self.len() <= index {
            return None;
        }

        // There are three main cases:
        //  Elements are contiguous
        //  Elements are discontiguous and the removal is in the tail section
        //  Elements are discontiguous and the removal is in the head section
        //      - special case when elements are technically contiguous,
        //        but self.head = 0
        //
        // For each of those there are two more cases:
        //  Insert is closer to tail
        //  Insert is closer to head
        //
        // Key: H - self.head
        //      T - self.tail
        //      o - Valid element
        //      x - Element marked for removal
        //      R - Indicates element that is being removed
        //      M - Indicates element was moved

        let idx = Self::wrap_add(self.tail(), index);

        let elem = unsafe { Some(self.buffer_read(idx)) };

        let distance_to_tail = index;
        let distance_to_head = self.len() - index;

        let contiguous = self.is_contiguous();

        match (contiguous, distance_to_tail <= distance_to_head, idx >= self.tail()) {
            (true, true, _) => {
                unsafe {
                    // contiguous, remove closer to tail:
                    //
                    //             T   R         H
                    //      [. . . o o x o o o o . . . . . .]
                    //
                    //               T           H
                    //      [. . . . o o o o o o . . . . . .]
                    //               M M

                    let tail = self.tail();
                    self.copy(tail + 1, tail, index);
                    self.set_tail(tail + 1);
                }
            }
            (true, false, _) => {
                unsafe {
                    // contiguous, remove closer to head:
                    //
                    //             T       R     H
                    //      [. . . o o o o x o o . . . . . .]
                    //
                    //             T           H
                    //      [. . . o o o o o o . . . . . . .]
                    //                     M M

                    let head = self.head();
                    self.copy(idx, idx + 1, head - idx - 1);
                    self.set_head(head - 1);
                }
            }
            (false, true, true) => {
                unsafe {
                    // discontiguous, remove closer to tail, tail section:
                    //
                    //                   H         T   R
                    //      [o o o o o o . . . . . o o x o o]
                    //
                    //                   H           T
                    //      [o o o o o o . . . . . . o o o o]
                    //                               M M

                    let tail = self.tail();
                    self.copy(tail + 1, tail, index);
                    let new_tail = Self::wrap_add(self.tail(), 1);
                    self.set_tail(new_tail);
                }
            }
            (false, false, false) => {
                unsafe {
                    // discontiguous, remove closer to head, head section:
                    //
                    //               R     H           T
                    //      [o o o o x o o . . . . . . o o o]
                    //
                    //                   H             T
                    //      [o o o o o o . . . . . . . o o o]
                    //               M M

                    let head = self.head();
                    self.copy(idx, idx + 1, head - idx - 1);
                    self.set_head(head - 1);
                }
            }
            (false, false, true) => {
                unsafe {
                    // discontiguous, remove closer to head, tail section:
                    //
                    //             H           T         R
                    //      [o o o . . . . . . o o o o o x o]
                    //
                    //           H             T
                    //      [o o . . . . . . . o o o o o o o]
                    //       M M                         M M
                    //
                    // or quasi-discontiguous, remove next to head, tail section:
                    //
                    //       H                 T         R
                    //      [. . . . . . . . . o o o o o x o]
                    //
                    //                         T           H
                    //      [. . . . . . . . . o o o o o o .]
                    //                                   M

                    // draw in elements in the tail section
                    self.copy(idx, idx + 1, A::capacity() - idx - 1);

                    // Prevents underflow.
                    if self.head() != 0 {
                        // copy first element into empty spot
                        self.copy(A::capacity() - 1, 0, 1);

                        // move elements in the head section backwards
                        let head = self.head();
                        self.copy(0, 1, head - 1);
                    }

                    let new_head = Self::wrap_sub(self.head(), 1);
                    self.set_head(new_head);
                }
            }
            (false, true, false) => {
                unsafe {
                    // discontiguous, remove closer to tail, head section:
                    //
                    //           R               H     T
                    //      [o o x o o o o o o o . . . o o o]
                    //
                    //                           H       T
                    //      [o o o o o o o o o o . . . . o o]
                    //       M M M                       M M

                    let tail = self.tail();
                    // draw in elements up to idx
                    self.copy(1, 0, idx);

                    // copy last element into empty spot
                    self.copy(0, A::capacity() - 1, 1);

                    // move elements from tail to end forward, excluding the last one
                    self.copy(tail + 1, tail, A::capacity() - tail - 1);

                    let new_tail = Self::wrap_add(tail, 1);
                    self.set_tail(new_tail);
                }
            }
        }

        return elem;
    }

    /// Splits the collection into two at the given index.
    ///
    /// Returns a newly allocated `Self`. `self` contains elements `[0, at)`,
    /// and the returned `Self` contains elements `[at, len)`.
    ///
    /// Element at index 0 is the front of the queue.
    ///
    /// # Panics
    ///
    /// Panics if `at > len`
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 4]> = vec![1,2,3].into_iter().collect();
    /// let buf2 = buf.split_off(1);
    /// // buf = [1], buf2 = [2, 3]
    /// assert_eq!(buf.len(), 1);
    /// assert_eq!(buf2.len(), 2);
    /// ```
    #[inline]
    pub fn split_off(&mut self, at: usize) -> Self {
        let len = self.len();
        assert!(at <= len, "`at` out of bounds");

        let other_len = len - at;
        let mut other = Self::new();

        unsafe {
            let (first_half, second_half) = self.as_slices();

            let first_len = first_half.len();
            let second_len = second_half.len();
            if at < first_len {
                // `at` lies in the first half.
                let amount_in_first = first_len - at;

                ptr::copy_nonoverlapping(first_half.as_ptr().offset(at as isize),
                                         other.ptr_mut(),
                                         amount_in_first);

                // just take all of the second half.
                ptr::copy_nonoverlapping(second_half.as_ptr(),
                                         other.ptr_mut().offset(amount_in_first as isize),
                                         second_len);
            } else {
                // `at` lies in the second half, need to factor in the elements we skipped
                // in the first half.
                let offset = at - first_len;
                let amount_in_second = second_len - offset;
                ptr::copy_nonoverlapping(second_half.as_ptr().offset(offset as isize),
                                         other.ptr_mut(),
                                         amount_in_second);
            }
        }

        // Cleanup where the ends of the buffers are
        unsafe {
            let head = self.head();
            self.set_head(Self::wrap_sub(head, other_len));
            other.set_head(other_len);
        }

        other
    }

    /// Moves all the elements of `other` into `Self`, leaving `other` empty.
    ///
    /// Does not extract more items than there is space for. No error
    /// occurs if there are more iterator elements.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 8]> = vec![1, 2, 3].into_iter().collect();
    /// // buf2: ArrayDeque<[_; 8]> can be infer
    /// let mut buf2 = vec![4, 5, 6].into_iter().collect();
    /// let mut buf3 = vec![7, 8, 9].into_iter().collect();
    ///
    /// buf.append(&mut buf2);
    /// assert_eq!(buf.len(), 6);
    /// assert_eq!(buf2.len(), 0);
    ///
    /// // max capacity reached
    /// buf.append(&mut buf3);
    /// assert_eq!(buf.len(), 7);
    /// assert_eq!(buf3.len(), 0);
    /// ```
    pub fn append(&mut self, other: &mut Self) {
        self.extend(other.drain(..));
    }

    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&e)` returns false.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    ///
    /// # Examples
    ///
    /// ```
    /// use arraydeque::ArrayDeque;
    ///
    /// let mut buf: ArrayDeque<[_; 5]> = ArrayDeque::new();
    /// buf.extend(1..5);
    /// buf.retain(|&x| x%2 == 0);
    ///
    /// let v: Vec<_> = buf.into_iter().collect();
    /// assert_eq!(&v[..], &[2, 4]);
    /// ```
    pub fn retain<F>(&mut self, mut f: F)
        where F: FnMut(&A::Item) -> bool
    {
        let len = self.len();
        let mut del = 0;
        for i in 0..len {
            if !f(&self[i]) {
                del += 1;
            } else if del > 0 {
                self.swap(i - del, i);
            }
        }
        if del > 0 {
            for _ in (len - del)..self.len() {
                self.pop_back();
            }
        }
    }
}

impl<A: Array> PartialEq for ArrayDeque<A>
    where A::Item: PartialEq
{
    fn eq(&self, other: &ArrayDeque<A>) -> bool {
        if self.len() != other.len() {
            return false;
        }
        let (sa, sb) = self.as_slices();
        let (oa, ob) = other.as_slices();
        if sa.len() == oa.len() {
            sa == oa && sb == ob
        } else if sa.len() < oa.len() {
            // Always divisible in three sections, for example:
            // self:  [a b c|d e f]
            // other: [0 1 2 3|4 5]
            // front = 3, mid = 1,
            // [a b c] == [0 1 2] && [d] == [3] && [e f] == [4 5]
            let front = sa.len();
            let mid = oa.len() - front;

            let (oa_front, oa_mid) = oa.split_at(front);
            let (sb_mid, sb_back) = sb.split_at(mid);
            debug_assert_eq!(sa.len(), oa_front.len());
            debug_assert_eq!(sb_mid.len(), oa_mid.len());
            debug_assert_eq!(sb_back.len(), ob.len());
            sa == oa_front && sb_mid == oa_mid && sb_back == ob
        } else {
            let front = oa.len();
            let mid = sa.len() - front;

            let (sa_front, sa_mid) = sa.split_at(front);
            let (ob_mid, ob_back) = ob.split_at(mid);
            debug_assert_eq!(sa_front.len(), oa.len());
            debug_assert_eq!(sa_mid.len(), ob_mid.len());
            debug_assert_eq!(sb.len(), ob_back.len());
            sa_front == oa && sa_mid == ob_mid && sb == ob_back
        }
    }
}

impl<A: Array> Eq for ArrayDeque<A> where A::Item: Eq {}

impl<A: Array> PartialOrd for ArrayDeque<A>
    where A::Item: PartialOrd
{
    fn partial_cmp(&self, other: &ArrayDeque<A>) -> Option<Ordering> {
        self.iter().partial_cmp(other.iter())
    }
}

impl<A: Array> Ord for ArrayDeque<A>
    where A::Item: Ord
{
    #[inline]
    fn cmp(&self, other: &ArrayDeque<A>) -> Ordering {
        self.iter().cmp(other.iter())
    }
}

impl<A: Array> Hash for ArrayDeque<A>
    where A::Item: Hash
{
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.len().hash(state);
        let (a, b) = self.as_slices();
        Hash::hash_slice(a, state);
        Hash::hash_slice(b, state);
    }
}

impl<A: Array> Index<usize> for ArrayDeque<A> {
    type Output = A::Item;

    #[inline]
    fn index(&self, index: usize) -> &A::Item {
        let len = self.len();
        self.get(index)
            .or_else(|| {
                panic!("index out of bounds: the len is {} but the index is {}",
                       len,
                       index)
            })
            .unwrap()
    }
}

impl<A: Array> IndexMut<usize> for ArrayDeque<A> {
    #[inline]
    fn index_mut(&mut self, index: usize) -> &mut A::Item {
        let len = self.len();
        self.get_mut(index)
            .or_else(|| {
                panic!("index out of bounds: the len is {} but the index is {}",
                       len,
                       index)
            })
            .unwrap()
    }
}

impl<A: Array> iter::FromIterator<A::Item> for ArrayDeque<A> {
    fn from_iter<T: IntoIterator<Item = A::Item>>(iter: T) -> Self {
        let mut array = ArrayDeque::new();
        array.extend(iter);
        array
    }
}

impl<A: Array> IntoIterator for ArrayDeque<A> {
    type Item = A::Item;
    type IntoIter = IntoIter<A>;

    fn into_iter(self) -> IntoIter<A> {
        IntoIter { inner: self }
    }
}

impl<'a, A: Array> IntoIterator for &'a ArrayDeque<A> {
    type Item = &'a A::Item;
    type IntoIter = Iter<'a, A::Item>;

    fn into_iter(self) -> Iter<'a, A::Item> {
        self.iter()
    }
}

impl<'a, A: Array> IntoIterator for &'a mut ArrayDeque<A> {
    type Item = &'a mut A::Item;
    type IntoIter = IterMut<'a, A::Item>;

    fn into_iter(mut self) -> IterMut<'a, A::Item> {
        self.iter_mut()
    }
}

/// Extend the `ArrayDeque` with an iterator.
///
/// Does not extract more items than there is space for. No error
/// occurs if there are more iterator elements.
impl<A: Array> Extend<A::Item> for ArrayDeque<A> {
    fn extend<T: IntoIterator<Item = A::Item>>(&mut self, iter: T) {
        let take = self.capacity() - self.len();
        for elt in iter.into_iter().take(take) {
            self.push_back(elt);
        }
    }
}

impl<A: Array> fmt::Debug for ArrayDeque<A>
    where A::Item: fmt::Debug
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_list().entries(self).finish()
    }
}

#[inline]
fn wrap_add(index: usize, addend: usize, capacity: usize) -> usize {
    debug_assert!(addend <= capacity);
    (index + addend) % capacity
}

#[inline]
fn wrap_sub(index: usize, subtrahend: usize, capacity: usize) -> usize {
    debug_assert!(subtrahend <= capacity);
    (index + capacity - subtrahend) % capacity
}

#[inline]
fn count(tail: usize, head: usize, capacity: usize) -> usize {
    debug_assert!(head < capacity);
    debug_assert!(tail < capacity);
    if head >= tail {
        head - tail
    } else {
        capacity + head - tail
    }
}

/// `ArrayDeque` iterator
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
#[derive(Clone)]
pub struct Iter<'a, T: 'a> {
    ring: &'a [T],
    head: usize,
    tail: usize,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    #[inline]
    fn next(&mut self) -> Option<&'a T> {
        if self.tail == self.head {
            return None;
        }
        let tail = self.tail;
        self.tail = wrap_add(self.tail, 1, self.ring.len());
        unsafe { Some(self.ring.get_unchecked(tail)) }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = count(self.tail, self.head, self.ring.len());
        (len, Some(len))
    }
}

impl<'a, T> DoubleEndedIterator for Iter<'a, T> {
    #[inline]
    fn next_back(&mut self) -> Option<&'a T> {
        if self.tail == self.head {
            return None;
        }
        self.head = wrap_sub(self.head, 1, self.ring.len());
        unsafe { Some(self.ring.get_unchecked(self.head)) }
    }
}

impl<'a, T> ExactSizeIterator for Iter<'a, T> {}

/// `ArrayDeque` mutable iterator
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct IterMut<'a, T: 'a> {
    ring: &'a mut [T],
    head: usize,
    tail: usize,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    #[inline]
    fn next(&mut self) -> Option<&'a mut T> {
        if self.tail == self.head {
            return None;
        }
        let tail = self.tail;
        self.tail = wrap_add(self.tail, 1, self.ring.len());

        unsafe {
            let elem = self.ring.get_unchecked_mut(tail);
            Some(&mut *(elem as *mut _))
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = count(self.tail, self.head, self.ring.len());
        (len, Some(len))
    }
}

impl<'a, T> DoubleEndedIterator for IterMut<'a, T> {
    #[inline]
    fn next_back(&mut self) -> Option<&'a mut T> {
        if self.tail == self.head {
            return None;
        }
        self.head = wrap_sub(self.head, 1, self.ring.len());

        unsafe {
            let elem = self.ring.get_unchecked_mut(self.head);
            Some(&mut *(elem as *mut _))
        }
    }
}

impl<'a, T> ExactSizeIterator for IterMut<'a, T> {}

/// By-value `ArrayDeque` iterator
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct IntoIter<A: Array> {
    inner: ArrayDeque<A>,
}

impl<A: Array> Iterator for IntoIter<A> {
    type Item = A::Item;

    #[inline]
    fn next(&mut self) -> Option<A::Item> {
        self.inner.pop_front()
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.inner.len();
        (len, Some(len))
    }
}

impl<A: Array> DoubleEndedIterator for IntoIter<A> {
    #[inline]
    fn next_back(&mut self) -> Option<A::Item> {
        self.inner.pop_back()
    }
}

impl<A: Array> ExactSizeIterator for IntoIter<A> {}

/// Draining `ArrayDeque` iterator
pub struct Drain<'a, A>
    where A: Array,
          A::Item: 'a
{
    after_tail: usize,
    after_head: usize,
    iter: Iter<'a, A::Item>,
    deque: *mut ArrayDeque<A>,
}

impl<'a, A> Drop for Drain<'a, A>
    where A: Array,
          A::Item: 'a
{
    fn drop(&mut self) {
        for _ in self.by_ref() {}

        let source_deque = unsafe { &mut *self.deque };

        // T = source_deque_tail; H = source_deque_head; t = drain_tail; h = drain_head
        //
        //        T   t   h   H
        // [. . . o o x x o o . . .]
        //
        let orig_tail = source_deque.tail();
        let drain_tail = source_deque.head();
        let drain_head = self.after_tail;
        let orig_head = self.after_head;

        let tail_len = count(orig_tail, drain_tail, A::capacity());
        let head_len = count(drain_head, orig_head, A::capacity());

        // Restore the original head value
        unsafe { source_deque.set_head(orig_head) }
        match (tail_len, head_len) {
            (0, 0) => {
                unsafe { source_deque.set_head(0) }
                unsafe { source_deque.set_tail(0) }
            }
            (0, _) => unsafe { source_deque.set_tail(drain_head) },
            (_, 0) => unsafe { source_deque.set_head(drain_tail) },
            _ => unsafe {
                if tail_len <= head_len {
                    let new_tail = ArrayDeque::<A>::wrap_sub(drain_head, tail_len);
                    source_deque.set_tail(new_tail);
                    source_deque.wrap_copy(new_tail, orig_tail, tail_len);
                } else {
                    let new_head = ArrayDeque::<A>::wrap_add(drain_tail, head_len);
                    source_deque.set_head(new_head);
                    source_deque.wrap_copy(drain_tail, drain_head, head_len);
                }
            },
        }
    }
}

impl<'a, A> Iterator for Drain<'a, A>
    where A: Array,
          A::Item: 'a
{
    type Item = A::Item;

    #[inline]
    fn next(&mut self) -> Option<A::Item> {
        self.iter.next().map(|elt| unsafe { ptr::read(elt) })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, A> DoubleEndedIterator for Drain<'a, A>
    where A: Array,
          A::Item: 'a
{
    #[inline]
    fn next_back(&mut self) -> Option<A::Item> {
        self.iter.next_back().map(|elt| unsafe { ptr::read(elt) })
    }
}

impl<'a, A> ExactSizeIterator for Drain<'a, A>
    where A: Array,
          A::Item: 'a
{
}

#[cfg(test)]
mod tests {
    use super::ArrayDeque;
    use std::vec::Vec;

    #[test]
    fn test_simple() {
        let mut tester: ArrayDeque<[_; 8]> = ArrayDeque::new();
        assert_eq!(tester.capacity(), 7);
        assert_eq!(tester.len(), 0);

        tester.push_back(1);
        tester.push_back(2);
        tester.push_back(3);
        tester.push_back(4);
        assert_eq!(tester.len(), 4);

        assert_eq!(tester.pop_front(), Some(1));
        assert_eq!(tester.pop_front(), Some(2));
        assert_eq!(tester.len(), 2);
        assert_eq!(tester.pop_front(), Some(3));
        assert_eq!(tester.pop_front(), Some(4));
        assert_eq!(tester.pop_front(), None);
    }

    #[test]
    fn test_simple_reversely() {
        let mut tester: ArrayDeque<[_; 8]> = ArrayDeque::new();
        assert_eq!(tester.capacity(), 7);
        assert_eq!(tester.len(), 0);

        tester.push_front(1);
        tester.push_front(2);
        tester.push_front(3);
        tester.push_front(4);
        assert_eq!(tester.len(), 4);
        assert_eq!(tester.pop_back(), Some(1));
        assert_eq!(tester.pop_back(), Some(2));
        assert_eq!(tester.len(), 2);
        assert_eq!(tester.pop_back(), Some(3));
        assert_eq!(tester.pop_back(), Some(4));
        assert_eq!(tester.pop_back(), None);
    }

    #[test]
    fn test_overflow() {
        let mut tester: ArrayDeque<[_; 3]> = ArrayDeque::new();
        assert_eq!(tester.push_back(1), None);
        assert_eq!(tester.push_back(2), None);
        assert_eq!(tester.push_back(3), Some(3));
    }

    #[test]
    fn test_pop_empty() {
        let mut tester: ArrayDeque<[_; 3]> = ArrayDeque::new();
        assert_eq!(tester.push_back(1), None);
        assert_eq!(tester.pop_front(), Some(1));
        assert_eq!(tester.is_empty(), true);
        assert_eq!(tester.len(), 0);
        assert_eq!(tester.pop_front(), None);
    }

    #[test]
    fn test_index() {
        let mut tester: ArrayDeque<[_; 4]> = ArrayDeque::new();
        tester.push_back(1);
        tester.push_back(2);
        tester.push_back(3);
        assert_eq!(tester[0], 1);
        // pop_front 1 <- [2, 3]
        assert_eq!(tester.pop_front(), Some(1));
        assert_eq!(tester[0], 2);
        assert_eq!(tester.len(), 2);
        // push_front 0 -> [0, 2, 3]
        tester.push_front(0);
        assert_eq!(tester[0], 0);
        // [0, 2] -> 3 pop_back
        assert_eq!(tester.pop_back(), Some(3));
        assert_eq!(tester[1], 2);
    }

    #[test]
    #[should_panic]
    fn test_index_overflow() {
        let mut tester: ArrayDeque<[_; 4]> = ArrayDeque::new();
        tester.push_back(1);
        tester.push_back(2);
        tester[2];
    }

    #[test]
    fn test_iter() {
        let mut tester: ArrayDeque<[_; 3]> = ArrayDeque::new();
        tester.push_back(1);
        tester.push_back(2);
        {
            let mut iter = tester.iter();
            assert_eq!(iter.size_hint(), (2, Some(2)));
            assert_eq!(iter.next(), Some(&1));
            assert_eq!(iter.next(), Some(&2));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }
        tester.pop_front();
        tester.push_back(3);
        {
            let mut iter = (&tester).into_iter();
            assert_eq!(iter.next(), Some(&2));

            // test clone
            let mut iter2 = iter.clone();
            assert_eq!(iter.next(), Some(&3));
            assert_eq!(iter.next(), None);
            assert_eq!(iter2.next(), Some(&3));
            assert_eq!(iter2.next(), None);
        }
    }

    #[test]
    fn test_iter_mut() {
        let mut tester: ArrayDeque<[_; 3]> = ArrayDeque::new();
        tester.push_back(1);
        tester.push_back(2);
        {
            let mut iter = tester.iter_mut();
            assert_eq!(iter.size_hint(), (2, Some(2)));
            assert_eq!(iter.next(), Some(&mut 1));
            assert_eq!(iter.next(), Some(&mut 2));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }
        tester.pop_front();
        tester.push_back(3);
        {
            let mut iter = (&mut tester).into_iter();
            assert_eq!(iter.next(), Some(&mut 2));
            assert_eq!(iter.next(), Some(&mut 3));
            assert_eq!(iter.next(), None);
        }
        {
            // mutation
            let mut iter = tester.iter_mut();
            iter.next().map(|n| *n += 1);
            iter.next().map(|n| *n += 2);
        }
        assert_eq!(tester[0], 3);
        assert_eq!(tester[1], 5);
    }

    #[test]
    fn test_into_iter() {
        #[derive(Eq, PartialEq, Debug)]
        struct NoCopy<T>(T);

        {
            let mut tester: ArrayDeque<[NoCopy<u8>; 3]> = ArrayDeque::new();
            tester.push_back(NoCopy(1));
            tester.push_back(NoCopy(2));
            let mut iter = tester.into_iter();
            assert_eq!(iter.size_hint(), (2, Some(2)));
            assert_eq!(iter.next(), Some(NoCopy(1)));
            assert_eq!(iter.next(), Some(NoCopy(2)));
            assert_eq!(iter.next(), None);
            assert_eq!(iter.size_hint(), (0, Some(0)));
        }
        {
            let mut tester: ArrayDeque<[NoCopy<u8>; 3]> = ArrayDeque::new();
            tester.push_back(NoCopy(1));
            tester.push_back(NoCopy(2));
            tester.pop_front();
            tester.push_back(NoCopy(3));
            let mut iter = tester.into_iter();
            assert_eq!(iter.next(), Some(NoCopy(2)));
            assert_eq!(iter.next(), Some(NoCopy(3)));
            assert_eq!(iter.next(), None);
        }
        {
            let mut tester: ArrayDeque<[NoCopy<u8>; 3]> = ArrayDeque::new();
            tester.push_back(NoCopy(1));
            tester.push_back(NoCopy(2));
            tester.pop_front();
            tester.push_back(NoCopy(3));
            tester.pop_front();
            tester.push_back(NoCopy(4));
            let mut iter = tester.into_iter();
            assert_eq!(iter.next(), Some(NoCopy(3)));
            assert_eq!(iter.next(), Some(NoCopy(4)));
            assert_eq!(iter.next(), None);
        }
    }

    #[test]
    fn test_drain() {
        const CAP: usize = 7;
        let mut tester: ArrayDeque<[_; CAP + 1]> = ArrayDeque::new();

        for padding in 0..CAP {
            for drain_start in 0..CAP {
                for drain_end in drain_start..CAP {
                    // deque starts from different tail position
                    unsafe {
                        tester.set_head(padding);
                        tester.set_tail(padding);
                    }

                    tester.extend(0..CAP);

                    let mut expected = vec![0, 1, 2, 3, 4, 5, 6];
                    let drains: Vec<_> = tester.drain(drain_start..drain_end).collect();
                    let expected_drains: Vec<_> = expected.drain(drain_start..drain_end).collect();
                    assert_eq!(drains, expected_drains);
                    assert_eq!(tester.len(), expected.len());
                }
            }
        }
    }

    #[test]
    fn test_drop() {
        use std::cell::Cell;

        let flag = &Cell::new(0);

        struct Bump<'a>(&'a Cell<i32>);

        impl<'a> Drop for Bump<'a> {
            fn drop(&mut self) {
                let n = self.0.get();
                self.0.set(n + 1);
            }
        }

        {
            let mut tester = ArrayDeque::<[Bump; 128]>::new();
            tester.push_back(Bump(flag));
            tester.push_back(Bump(flag));
        }
        assert_eq!(flag.get(), 2);

        // test something with the nullable pointer optimization
        flag.set(0);
        {
            let mut tester = ArrayDeque::<[_; 4]>::new();
            tester.push_back(vec![Bump(flag)]);
            tester.push_back(vec![Bump(flag), Bump(flag)]);
            tester.push_back(vec![]);
            tester.push_back(vec![Bump(flag)]);
            assert_eq!(flag.get(), 1);
            drop(tester.pop_back());
            assert_eq!(flag.get(), 1);
            drop(tester.pop_back());
            assert_eq!(flag.get(), 3);
        }
        assert_eq!(flag.get(), 4);
    }

    #[test]
    fn test_as_slice() {
        const CAP: usize = 10;
        let mut tester = ArrayDeque::<[_; CAP]>::new();

        for len in 0..CAP - 1 {
            for padding in 0..CAP {
                // deque starts from different tail position
                unsafe {
                    tester.set_head(padding);
                    tester.set_tail(padding);
                }

                let mut expected = vec![];
                tester.extend(0..len);
                expected.extend(0..len);

                let split_idx = CAP - padding;
                if split_idx < len {
                    assert_eq!(tester.as_slices(), expected[..].split_at(split_idx));
                } else {
                    assert_eq!(tester.as_slices(), (&expected[..], &[][..]));
                }
            }
        }
    }

    #[test]
    fn test_partial_equal() {
        const CAP: usize = 10;
        let mut tester = ArrayDeque::<[f64; CAP]>::new();

        for len in 0..CAP - 1 {
            for padding in 0..CAP {
                // deque starts from different tail position
                unsafe {
                    tester.set_head(padding);
                    tester.set_tail(padding);
                }

                let mut expected = ArrayDeque::<[f64; CAP]>::new();
                for x in 0..len {
                    tester.push_back(x as f64);
                    expected.push_back(x as f64);
                }
                assert_eq!(tester, expected);

                // test negative
                if len > 2 {
                    tester.pop_front();
                    expected.pop_back();
                    assert!(tester != expected);
                }
            }
        }
    }

    #[test]
    fn test_fmt() {
        let mut tester = ArrayDeque::<[_; 5]>::new();
        tester.extend(0..4);
        assert_eq!(format!("{:?}", tester), "[0, 1, 2, 3]");
    }

    #[test]
    fn test_from_iterator() {
        let tester: ArrayDeque<[_; 5]>;
        let mut expected = ArrayDeque::<[_; 5]>::new();
        tester = vec![0, 1, 2, 3].into_iter().collect();
        expected.extend(0..4);
        assert_eq!(tester, expected);
    }

    #[test]
    fn test_extend() {
        let mut tester: ArrayDeque<[usize; 10]>;
        tester = vec![0, 1, 2, 3].into_iter().collect();
        tester.extend(vec![4, 5, 6, 7]);
        let expected = (0..8).collect::<ArrayDeque<[usize; 10]>>();
        assert_eq!(tester, expected);
    }

    #[test]
    fn test_swap_front_back_remove() {
        fn test(back: bool) {
            const CAP: usize = 16;
            let mut tester = ArrayDeque::<[_; CAP]>::new();
            let usable_cap = tester.capacity();
            let final_len = usable_cap / 2;

            for len in 0..final_len {
                let expected = if back {
                    (0..len).collect()
                } else {
                    (0..len).rev().collect()
                };
                for padding in 0..usable_cap {
                    unsafe {
                        tester.set_tail(padding);
                        tester.set_head(padding);
                    }
                    if back {
                        for i in 0..len * 2 {
                            tester.push_front(i);
                        }
                        for i in 0..len {
                            assert_eq!(tester.swap_remove_back(i), Some(len * 2 - 1 - i));
                        }
                    } else {
                        for i in 0..len * 2 {
                            tester.push_back(i);
                        }
                        for i in 0..len {
                            let idx = tester.len() - 1 - i;
                            assert_eq!(tester.swap_remove_front(idx), Some(len * 2 - 1 - i));
                        }
                    }
                    assert!(tester.tail() < CAP);
                    assert!(tester.head() < CAP);
                    assert_eq!(tester, expected);
                }
            }
        }
        test(true);
        test(false);
    }

    #[test]
    fn test_retain() {
        const CAP: usize = 10;
        let mut tester: ArrayDeque<[_; CAP]> = ArrayDeque::new();
        for padding in 0..CAP {
            unsafe {
                tester.set_tail(padding);
                tester.set_head(padding);
            }
            tester.extend(0..CAP);
            tester.retain(|x| x % 2 == 0);
            assert_eq!(tester.iter().count(), CAP / 2);
        }
    }

    #[test]
    fn test_append() {
        let mut a: ArrayDeque<[_; 16]> = vec![1, 2, 3].into_iter().collect();
        let mut b: ArrayDeque<[_; 16]> = vec![4, 5, 6].into_iter().collect();

        // normal append
        a.append(&mut b);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), []);

        // append nothing to something
        a.append(&mut b);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), []);

        // append something to nothing
        b.append(&mut a);
        assert_eq!(b.iter().cloned().collect::<Vec<_>>(), [1, 2, 3, 4, 5, 6]);
        assert_eq!(a.iter().cloned().collect::<Vec<_>>(), []);
    }

    #[test]
    fn test_split_off() {
        const CAP: usize = 16;
        let mut tester = ArrayDeque::<[_; CAP]>::new();
        for len in 0..CAP {
            // index to split at
            for at in 0..len + 1 {
                for padding in 0..CAP {
                    let expected_self = (0..).take(at).collect();
                    let expected_other = (at..).take(len - at).collect();
                    unsafe {
                        tester.set_head(padding);
                        tester.set_tail(padding);
                    }
                    for i in 0..len {
                        tester.push_back(i);
                    }
                    let result = tester.split_off(at);
                    assert!(tester.tail() < CAP);
                    assert!(tester.head() < CAP);
                    assert!(result.tail() < CAP);
                    assert!(result.head() < CAP);
                    assert_eq!(tester, expected_self);
                    assert_eq!(result, expected_other);
                }
            }
        }
    }

    #[test]
    fn test_insert() {
        const CAP: usize = 16;
        let mut tester = ArrayDeque::<[_; CAP]>::new();

        // len is the length *after* insertion
        for len in 1..CAP {
            // 0, 1, 2, .., len - 1
            let expected = (0..).take(len).collect();
            for padding in 0..CAP {
                for to_insert in 0..len {
                    unsafe {
                        tester.set_tail(padding);
                        tester.set_head(padding);
                    }
                    for i in 0..len {
                        if i != to_insert {
                            tester.push_back(i);
                        }
                    }
                    tester.insert(to_insert, to_insert);
                    assert!(tester.tail() < CAP);
                    assert!(tester.head() < CAP);
                    assert_eq!(tester, expected);
                }
            }
        }
    }

    #[test]
    fn test_remove() {
        const CAP: usize = 16;
        let mut tester = ArrayDeque::<[_; CAP]>::new();

        // len is the length *after* removal
        for len in 0..CAP - 1 {
            // 0, 1, 2, .., len - 1
            let expected = (0..).take(len).collect();
            for padding in 0..CAP {
                for to_remove in 0..len + 1 {
                    unsafe {
                        tester.set_tail(padding);
                        tester.set_head(padding);
                    }
                    for i in 0..len {
                        if i == to_remove {
                            tester.push_back(1234);
                        }
                        tester.push_back(i);
                    }
                    if to_remove == len {
                        tester.push_back(1234);
                    }
                    tester.remove(to_remove);
                    assert!(tester.tail() < CAP);
                    assert!(tester.head() < CAP);
                    assert_eq!(tester, expected);
                }
            }
        }
    }

    #[test]
    fn test_clone() {
        let tester: ArrayDeque<[_; 16]> = (0..16).into_iter().collect();
        let cloned = tester.clone();
        assert_eq!(tester, cloned)
    }
}

#[cfg(test)]
#[cfg(feature = "use_generic_array")]
mod test_generic_array {
    extern crate generic_array;

    use generic_array::GenericArray;
    use generic_array::typenum::U41;

    use super::ArrayDeque;

    #[test]
    fn test_simple() {
        let mut vec: ArrayDeque<GenericArray<i32, U41>> = ArrayDeque::new();

        assert_eq!(vec.len(), 0);
        assert_eq!(vec.capacity(), 40);
        vec.extend(0..20);
        assert_eq!(vec.len(), 20);
        assert_eq!(vec.into_iter().take(5).collect::<Vec<_>>(), vec![0, 1, 2, 3, 4]);
    }
}
