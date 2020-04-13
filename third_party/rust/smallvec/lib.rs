// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Small vectors in various sizes. These store a certain number of elements inline, and fall back
//! to the heap for larger allocations.  This can be a useful optimization for improving cache
//! locality and reducing allocator traffic for workloads that fit within the inline buffer.
//!
//! ## `no_std` support
//!
//! By default, `smallvec` does not depend on `std`.  However, the optional
//! `write` feature implements the `std::io::Write` trait for vectors of `u8`.
//! When this feature is enabled, `smallvec` depends on `std`.
//!
//! ## Optional features
//!
//! ### `write`
//!
//! When this feature is enabled, `SmallVec<[u8; _]>` implements the `std::io::Write` trait.
//! This feature is not compatible with `#![no_std]` programs.
//!
//! ### `union`
//!
//! **This feature is unstable and requires a nightly build of the Rust toolchain.**
//!
//! When the `union` feature is enabled `smallvec` will track its state (inline or spilled)
//! without the use of an enum tag, reducing the size of the `smallvec` by one machine word.
//! This means that there is potentially no space overhead compared to `Vec`.
//! Note that `smallvec` can still be larger than `Vec` if the inline buffer is larger than two
//! machine words.
//!
//! To use this feature add `features = ["union"]` in the `smallvec` section of Cargo.toml.
//! Note that this feature requires a nightly compiler (for now).
//!
//! ### `const_generics`
//!
//! **This feature is unstable and requires a nightly build of the Rust toolchain.**
//!
//! When this feature is enabled, `SmallVec` works with any arrays of any size, not just a fixed
//! list of sizes.
//! 
//! ### `specialization`
//!
//! **This feature is unstable and requires a nightly build of the Rust toolchain.**
//!
//! When this feature is enabled, `SmallVec::from(slice)` has improved performance for slices
//! of `Copy` types.  (Without this feature, you can use `SmallVec::from_slice` to get optimal
//! performance for `Copy` types.)
//!
//! ### `may_dangle`
//!
//! **This feature is unstable and requires a nightly build of the Rust toolchain.**
//!
//! This feature makes the Rust compiler less strict about use of vectors that contain borrowed
//! references. For details, see the
//! [Rustonomicon](https://doc.rust-lang.org/1.42.0/nomicon/dropck.html#an-escape-hatch).

#![no_std]
#![cfg_attr(feature = "union", feature(untagged_unions))]
#![cfg_attr(feature = "specialization", feature(specialization))]
#![cfg_attr(feature = "may_dangle", feature(dropck_eyepatch))]
#![cfg_attr(feature = "const_generics", allow(incomplete_features))]
#![cfg_attr(feature = "const_generics", feature(const_generics))]
#![deny(missing_docs)]

#[doc(hidden)]
pub extern crate alloc;

#[cfg(any(test, feature = "write"))]
extern crate std;

use alloc::boxed::Box;
use alloc::{vec, vec::Vec};
use core::borrow::{Borrow, BorrowMut};
use core::cmp;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::hint::unreachable_unchecked;
use core::iter::{repeat, FromIterator, FusedIterator, IntoIterator};
use core::mem;
use core::mem::MaybeUninit;
use core::ops::{self, RangeBounds};
use core::ptr::{self, NonNull};
use core::slice::{self, SliceIndex};

#[cfg(feature = "serde")]
use serde::{
    de::{Deserialize, Deserializer, SeqAccess, Visitor},
    ser::{Serialize, SerializeSeq, Serializer},
};

#[cfg(feature = "serde")]
use core::marker::PhantomData;

#[cfg(feature = "write")]
use std::io;

/// Creates a [`SmallVec`] containing the arguments.
///
/// `smallvec!` allows `SmallVec`s to be defined with the same syntax as array expressions.
/// There are two forms of this macro:
///
/// - Create a [`SmallVec`] containing a given list of elements:
///
/// ```
/// # #[macro_use] extern crate smallvec;
/// # use smallvec::SmallVec;
/// # fn main() {
/// let v: SmallVec<[_; 128]> = smallvec![1, 2, 3];
/// assert_eq!(v[0], 1);
/// assert_eq!(v[1], 2);
/// assert_eq!(v[2], 3);
/// # }
/// ```
///
/// - Create a [`SmallVec`] from a given element and size:
///
/// ```
/// # #[macro_use] extern crate smallvec;
/// # use smallvec::SmallVec;
/// # fn main() {
/// let v: SmallVec<[_; 0x8000]> = smallvec![1; 3];
/// assert_eq!(v, SmallVec::from_buf([1, 1, 1]));
/// # }
/// ```
///
/// Note that unlike array expressions this syntax supports all elements
/// which implement [`Clone`] and the number of elements doesn't have to be
/// a constant.
///
/// This will use `clone` to duplicate an expression, so one should be careful
/// using this with types having a nonstandard `Clone` implementation. For
/// example, `smallvec![Rc::new(1); 5]` will create a vector of five references
/// to the same boxed integer value, not five references pointing to independently
/// boxed integers.

#[macro_export]
macro_rules! smallvec {
    // count helper: transform any expression into 1
    (@one $x:expr) => (1usize);
    ($elem:expr; $n:expr) => ({
        $crate::SmallVec::from_elem($elem, $n)
    });
    ($($x:expr),*$(,)*) => ({
        let count = 0usize $(+ smallvec!(@one $x))*;
        let mut vec = $crate::SmallVec::new();
        if count <= vec.inline_size() {
            $(vec.push($x);)*
            vec
        } else {
            $crate::SmallVec::from_vec($crate::alloc::vec![$($x,)*])
        }
    });
}

/// `panic!()` in debug builds, optimization hint in release.
#[cfg(not(feature = "union"))]
macro_rules! debug_unreachable {
    () => {
        debug_unreachable!("entered unreachable code")
    };
    ($e:expr) => {
        if cfg!(not(debug_assertions)) {
            unreachable_unchecked();
        } else {
            panic!($e);
        }
    };
}

/// Trait to be implemented by a collection that can be extended from a slice
///
/// ## Example
///
/// ```rust
/// use smallvec::{ExtendFromSlice, SmallVec};
///
/// fn initialize<V: ExtendFromSlice<u8>>(v: &mut V) {
///     v.extend_from_slice(b"Test!");
/// }
///
/// let mut vec = Vec::new();
/// initialize(&mut vec);
/// assert_eq!(&vec, b"Test!");
///
/// let mut small_vec = SmallVec::<[u8; 8]>::new();
/// initialize(&mut small_vec);
/// assert_eq!(&small_vec as &[_], b"Test!");
/// ```
pub trait ExtendFromSlice<T> {
    /// Extends a collection from a slice of its element type
    fn extend_from_slice(&mut self, other: &[T]);
}

impl<T: Clone> ExtendFromSlice<T> for Vec<T> {
    fn extend_from_slice(&mut self, other: &[T]) {
        Vec::extend_from_slice(self, other)
    }
}

unsafe fn deallocate<T>(ptr: *mut T, capacity: usize) {
    let _vec: Vec<T> = Vec::from_raw_parts(ptr, 0, capacity);
    // Let it drop.
}

/// An iterator that removes the items from a `SmallVec` and yields them by value.
///
/// Returned from [`SmallVec::drain`][1].
///
/// [1]: struct.SmallVec.html#method.drain
pub struct Drain<'a, T: 'a + Array> {
    tail_start: usize,
    tail_len: usize,
    iter: slice::Iter<'a, T::Item>,
    vec: NonNull<SmallVec<T>>,
}

impl<'a, T: 'a + Array> fmt::Debug for Drain<'a, T>
where
    T::Item: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("Drain").field(&self.iter.as_slice()).finish()
    }
}

unsafe impl<'a, T: Sync + Array> Sync for Drain<'a, T> {}
unsafe impl<'a, T: Send + Array> Send for Drain<'a, T> {}

impl<'a, T: 'a + Array> Iterator for Drain<'a, T> {
    type Item = T::Item;

    #[inline]
    fn next(&mut self) -> Option<T::Item> {
        self.iter
            .next()
            .map(|reference| unsafe { ptr::read(reference) })
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, T: 'a + Array> DoubleEndedIterator for Drain<'a, T> {
    #[inline]
    fn next_back(&mut self) -> Option<T::Item> {
        self.iter
            .next_back()
            .map(|reference| unsafe { ptr::read(reference) })
    }
}

impl<'a, T: Array> ExactSizeIterator for Drain<'a, T> {
    #[inline]
    fn len(&self) -> usize {
        self.iter.len()
    }
}

impl<'a, T: Array> FusedIterator for Drain<'a, T> {}

impl<'a, T: 'a + Array> Drop for Drain<'a, T> {
    fn drop(&mut self) {
        self.for_each(drop);

        if self.tail_len > 0 {
            unsafe {
                let source_vec = self.vec.as_mut();

                // memmove back untouched tail, update to new length
                let start = source_vec.len();
                let tail = self.tail_start;
                if tail != start {
                    let src = source_vec.as_ptr().add(tail);
                    let dst = source_vec.as_mut_ptr().add(start);
                    ptr::copy(src, dst, self.tail_len);
                }
                source_vec.set_len(start + self.tail_len);
            }
        }
    }
}

#[cfg(feature = "union")]
union SmallVecData<A: Array> {
    inline: MaybeUninit<A>,
    heap: (*mut A::Item, usize),
}

#[cfg(feature = "union")]
impl<A: Array> SmallVecData<A> {
    #[inline]
    unsafe fn inline(&self) -> *const A::Item {
        self.inline.as_ptr() as *const A::Item
    }
    #[inline]
    unsafe fn inline_mut(&mut self) -> *mut A::Item {
        self.inline.as_mut_ptr() as *mut A::Item
    }
    #[inline]
    fn from_inline(inline: MaybeUninit<A>) -> SmallVecData<A> {
        SmallVecData { inline }
    }
    #[inline]
    unsafe fn into_inline(self) -> MaybeUninit<A> {
        self.inline
    }
    #[inline]
    unsafe fn heap(&self) -> (*mut A::Item, usize) {
        self.heap
    }
    #[inline]
    unsafe fn heap_mut(&mut self) -> &mut (*mut A::Item, usize) {
        &mut self.heap
    }
    #[inline]
    fn from_heap(ptr: *mut A::Item, len: usize) -> SmallVecData<A> {
        SmallVecData { heap: (ptr, len) }
    }
}

#[cfg(not(feature = "union"))]
enum SmallVecData<A: Array> {
    Inline(MaybeUninit<A>),
    Heap((*mut A::Item, usize)),
}

#[cfg(not(feature = "union"))]
impl<A: Array> SmallVecData<A> {
    #[inline]
    unsafe fn inline(&self) -> *const A::Item {
        match self {
            SmallVecData::Inline(a) => a.as_ptr() as *const A::Item,
            _ => debug_unreachable!(),
        }
    }
    #[inline]
    unsafe fn inline_mut(&mut self) -> *mut A::Item {
        match self {
            SmallVecData::Inline(a) => a.as_mut_ptr() as *mut A::Item,
            _ => debug_unreachable!(),
        }
    }
    #[inline]
    fn from_inline(inline: MaybeUninit<A>) -> SmallVecData<A> {
        SmallVecData::Inline(inline)
    }
    #[inline]
    unsafe fn into_inline(self) -> MaybeUninit<A> {
        match self {
            SmallVecData::Inline(a) => a,
            _ => debug_unreachable!(),
        }
    }
    #[inline]
    unsafe fn heap(&self) -> (*mut A::Item, usize) {
        match self {
            SmallVecData::Heap(data) => *data,
            _ => debug_unreachable!(),
        }
    }
    #[inline]
    unsafe fn heap_mut(&mut self) -> &mut (*mut A::Item, usize) {
        match self {
            SmallVecData::Heap(data) => data,
            _ => debug_unreachable!(),
        }
    }
    #[inline]
    fn from_heap(ptr: *mut A::Item, len: usize) -> SmallVecData<A> {
        SmallVecData::Heap((ptr, len))
    }
}

unsafe impl<A: Array + Send> Send for SmallVecData<A> {}
unsafe impl<A: Array + Sync> Sync for SmallVecData<A> {}

/// A `Vec`-like container that can store a small number of elements inline.
///
/// `SmallVec` acts like a vector, but can store a limited amount of data inline within the
/// `SmallVec` struct rather than in a separate allocation.  If the data exceeds this limit, the
/// `SmallVec` will "spill" its data onto the heap, allocating a new buffer to hold it.
///
/// The amount of data that a `SmallVec` can store inline depends on its backing store. The backing
/// store can be any type that implements the `Array` trait; usually it is a small fixed-sized
/// array.  For example a `SmallVec<[u64; 8]>` can hold up to eight 64-bit integers inline.
///
/// ## Example
///
/// ```rust
/// use smallvec::SmallVec;
/// let mut v = SmallVec::<[u8; 4]>::new(); // initialize an empty vector
///
/// // The vector can hold up to 4 items without spilling onto the heap.
/// v.extend(0..4);
/// assert_eq!(v.len(), 4);
/// assert!(!v.spilled());
///
/// // Pushing another element will force the buffer to spill:
/// v.push(4);
/// assert_eq!(v.len(), 5);
/// assert!(v.spilled());
/// ```
pub struct SmallVec<A: Array> {
    // The capacity field is used to determine which of the storage variants is active:
    // If capacity <= A::size() then the inline variant is used and capacity holds the current length of the vector (number of elements actually in use).
    // If capacity > A::size() then the heap variant is used and capacity holds the size of the memory allocation.
    capacity: usize,
    data: SmallVecData<A>,
}

impl<A: Array> SmallVec<A> {
    /// Construct an empty vector
    #[inline]
    pub fn new() -> SmallVec<A> {
        // Try to detect invalid custom implementations of `Array`. Hopefuly,
        // this check should be optimized away entirely for valid ones.
        assert!(
            mem::size_of::<A>() == A::size() * mem::size_of::<A::Item>()
                && mem::align_of::<A>() >= mem::align_of::<A::Item>()
        );
        SmallVec {
            capacity: 0,
            data: SmallVecData::from_inline(MaybeUninit::uninit()),
        }
    }

    /// Construct an empty vector with enough capacity pre-allocated to store at least `n`
    /// elements.
    ///
    /// Will create a heap allocation only if `n` is larger than the inline capacity.
    ///
    /// ```
    /// # use smallvec::SmallVec;
    ///
    /// let v: SmallVec<[u8; 3]> = SmallVec::with_capacity(100);
    ///
    /// assert!(v.is_empty());
    /// assert!(v.capacity() >= 100);
    /// ```
    #[inline]
    pub fn with_capacity(n: usize) -> Self {
        let mut v = SmallVec::new();
        v.reserve_exact(n);
        v
    }

    /// Construct a new `SmallVec` from a `Vec<A::Item>`.
    ///
    /// Elements will be copied to the inline buffer if vec.capacity() <= A::size().
    ///
    /// ```rust
    /// use smallvec::SmallVec;
    ///
    /// let vec = vec![1, 2, 3, 4, 5];
    /// let small_vec: SmallVec<[_; 3]> = SmallVec::from_vec(vec);
    ///
    /// assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
    /// ```
    #[inline]
    pub fn from_vec(mut vec: Vec<A::Item>) -> SmallVec<A> {
        if vec.capacity() <= A::size() {
            unsafe {
                let mut data = SmallVecData::<A>::from_inline(MaybeUninit::uninit());
                let len = vec.len();
                vec.set_len(0);
                ptr::copy_nonoverlapping(vec.as_ptr(), data.inline_mut(), len);

                SmallVec {
                    capacity: len,
                    data,
                }
            }
        } else {
            let (ptr, cap, len) = (vec.as_mut_ptr(), vec.capacity(), vec.len());
            mem::forget(vec);

            SmallVec {
                capacity: cap,
                data: SmallVecData::from_heap(ptr, len),
            }
        }
    }

    /// Constructs a new `SmallVec` on the stack from an `A` without
    /// copying elements.
    ///
    /// ```rust
    /// use smallvec::SmallVec;
    ///
    /// let buf = [1, 2, 3, 4, 5];
    /// let small_vec: SmallVec<_> = SmallVec::from_buf(buf);
    ///
    /// assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
    /// ```
    #[inline]
    pub fn from_buf(buf: A) -> SmallVec<A> {
        SmallVec {
            capacity: A::size(),
            data: SmallVecData::from_inline(MaybeUninit::new(buf)),
        }
    }

    /// Constructs a new `SmallVec` on the stack from an `A` without
    /// copying elements. Also sets the length, which must be less or
    /// equal to the size of `buf`.
    ///
    /// ```rust
    /// use smallvec::SmallVec;
    ///
    /// let buf = [1, 2, 3, 4, 5, 0, 0, 0];
    /// let small_vec: SmallVec<_> = SmallVec::from_buf_and_len(buf, 5);
    ///
    /// assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
    /// ```
    #[inline]
    pub fn from_buf_and_len(buf: A, len: usize) -> SmallVec<A> {
        assert!(len <= A::size());
        unsafe { SmallVec::from_buf_and_len_unchecked(MaybeUninit::new(buf), len) }
    }

    /// Constructs a new `SmallVec` on the stack from an `A` without
    /// copying elements. Also sets the length. The user is responsible
    /// for ensuring that `len <= A::size()`.
    ///
    /// ```rust
    /// use smallvec::SmallVec;
    /// use std::mem::MaybeUninit;
    ///
    /// let buf = [1, 2, 3, 4, 5, 0, 0, 0];
    /// let small_vec: SmallVec<_> = unsafe {
    ///     SmallVec::from_buf_and_len_unchecked(MaybeUninit::new(buf), 5)
    /// };
    ///
    /// assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
    /// ```
    #[inline]
    pub unsafe fn from_buf_and_len_unchecked(buf: MaybeUninit<A>, len: usize) -> SmallVec<A> {
        SmallVec {
            capacity: len,
            data: SmallVecData::from_inline(buf),
        }
    }

    /// Sets the length of a vector.
    ///
    /// This will explicitly set the size of the vector, without actually
    /// modifying its buffers, so it is up to the caller to ensure that the
    /// vector is actually the specified size.
    pub unsafe fn set_len(&mut self, new_len: usize) {
        let (_, len_ptr, _) = self.triple_mut();
        *len_ptr = new_len;
    }

    /// The maximum number of elements this vector can hold inline
    #[inline]
    pub fn inline_size(&self) -> usize {
        A::size()
    }

    /// The number of elements stored in the vector
    #[inline]
    pub fn len(&self) -> usize {
        self.triple().1
    }

    /// Returns `true` if the vector is empty
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The number of items the vector can hold without reallocating
    #[inline]
    pub fn capacity(&self) -> usize {
        self.triple().2
    }

    /// Returns a tuple with (data ptr, len, capacity)
    /// Useful to get all SmallVec properties with a single check of the current storage variant.
    #[inline]
    fn triple(&self) -> (*const A::Item, usize, usize) {
        unsafe {
            if self.spilled() {
                let (ptr, len) = self.data.heap();
                (ptr, len, self.capacity)
            } else {
                (self.data.inline(), self.capacity, A::size())
            }
        }
    }

    /// Returns a tuple with (data ptr, len ptr, capacity)
    #[inline]
    fn triple_mut(&mut self) -> (*mut A::Item, &mut usize, usize) {
        unsafe {
            if self.spilled() {
                let &mut (ptr, ref mut len_ptr) = self.data.heap_mut();
                (ptr, len_ptr, self.capacity)
            } else {
                (self.data.inline_mut(), &mut self.capacity, A::size())
            }
        }
    }

    /// Returns `true` if the data has spilled into a separate heap-allocated buffer.
    #[inline]
    pub fn spilled(&self) -> bool {
        self.capacity > A::size()
    }

    /// Creates a draining iterator that removes the specified range in the vector
    /// and yields the removed items.
    ///
    /// Note 1: The element range is removed even if the iterator is only
    /// partially consumed or not consumed at all.
    ///
    /// Note 2: It is unspecified how many elements are removed from the vector
    /// if the `Drain` value is leaked.
    ///
    /// # Panics
    ///
    /// Panics if the starting point is greater than the end point or if
    /// the end point is greater than the length of the vector.
    pub fn drain<R>(&mut self, range: R) -> Drain<'_, A>
    where
        R: RangeBounds<usize>,
    {
        use core::ops::Bound::*;

        let len = self.len();
        let start = match range.start_bound() {
            Included(&n) => n,
            Excluded(&n) => n + 1,
            Unbounded => 0,
        };
        let end = match range.end_bound() {
            Included(&n) => n + 1,
            Excluded(&n) => n,
            Unbounded => len,
        };

        assert!(start <= end);
        assert!(end <= len);

        unsafe {
            self.set_len(start);

            let range_slice = slice::from_raw_parts_mut(self.as_mut_ptr().add(start), end - start);

            Drain {
                tail_start: end,
                tail_len: len - end,
                iter: range_slice.iter(),
                vec: NonNull::from(self),
            }
        }
    }

    /// Append an item to the vector.
    #[inline]
    pub fn push(&mut self, value: A::Item) {
        unsafe {
            let (_, &mut len, cap) = self.triple_mut();
            if len == cap {
                self.reserve(1);
            }
            let (ptr, len_ptr, _) = self.triple_mut();
            *len_ptr = len + 1;
            ptr::write(ptr.add(len), value);
        }
    }

    /// Remove an item from the end of the vector and return it, or None if empty.
    #[inline]
    pub fn pop(&mut self) -> Option<A::Item> {
        unsafe {
            let (ptr, len_ptr, _) = self.triple_mut();
            if *len_ptr == 0 {
                return None;
            }
            let last_index = *len_ptr - 1;
            *len_ptr = last_index;
            Some(ptr::read(ptr.add(last_index)))
        }
    }

    /// Re-allocate to set the capacity to `max(new_cap, inline_size())`.
    ///
    /// Panics if `new_cap` is less than the vector's length.
    pub fn grow(&mut self, new_cap: usize) {
        unsafe {
            let (ptr, &mut len, cap) = self.triple_mut();
            let unspilled = !self.spilled();
            assert!(new_cap >= len);
            if new_cap <= self.inline_size() {
                if unspilled {
                    return;
                }
                self.data = SmallVecData::from_inline(MaybeUninit::uninit());
                ptr::copy_nonoverlapping(ptr, self.data.inline_mut(), len);
                self.capacity = len;
            } else if new_cap != cap {
                let mut vec = Vec::with_capacity(new_cap);
                let new_alloc = vec.as_mut_ptr();
                mem::forget(vec);
                ptr::copy_nonoverlapping(ptr, new_alloc, len);
                self.data = SmallVecData::from_heap(new_alloc, len);
                self.capacity = new_cap;
                if unspilled {
                    return;
                }
            } else {
                return;
            }
            deallocate(ptr, cap);
        }
    }

    /// Reserve capacity for `additional` more elements to be inserted.
    ///
    /// May reserve more space to avoid frequent reallocations.
    ///
    /// If the new capacity would overflow `usize` then it will be set to `usize::max_value()`
    /// instead. (This means that inserting `additional` new elements is not guaranteed to be
    /// possible after calling this function.)
    #[inline]
    pub fn reserve(&mut self, additional: usize) {
        // prefer triple_mut() even if triple() would work
        // so that the optimizer removes duplicated calls to it
        // from callers like insert()
        let (_, &mut len, cap) = self.triple_mut();
        if cap - len < additional {
            let new_cap = len
                .checked_add(additional)
                .and_then(usize::checked_next_power_of_two)
                .unwrap_or(usize::max_value());
            self.grow(new_cap);
        }
    }

    /// Reserve the minimum capacity for `additional` more elements to be inserted.
    ///
    /// Panics if the new capacity overflows `usize`.
    pub fn reserve_exact(&mut self, additional: usize) {
        let (_, &mut len, cap) = self.triple_mut();
        if cap - len < additional {
            match len.checked_add(additional) {
                Some(cap) => self.grow(cap),
                None => panic!("reserve_exact overflow"),
            }
        }
    }

    /// Shrink the capacity of the vector as much as possible.
    ///
    /// When possible, this will move data from an external heap buffer to the vector's inline
    /// storage.
    pub fn shrink_to_fit(&mut self) {
        if !self.spilled() {
            return;
        }
        let len = self.len();
        if self.inline_size() >= len {
            unsafe {
                let (ptr, len) = self.data.heap();
                self.data = SmallVecData::from_inline(MaybeUninit::uninit());
                ptr::copy_nonoverlapping(ptr, self.data.inline_mut(), len);
                deallocate(ptr, self.capacity);
                self.capacity = len;
            }
        } else if self.capacity() > len {
            self.grow(len);
        }
    }

    /// Shorten the vector, keeping the first `len` elements and dropping the rest.
    ///
    /// If `len` is greater than or equal to the vector's current length, this has no
    /// effect.
    ///
    /// This does not re-allocate.  If you want the vector's capacity to shrink, call
    /// `shrink_to_fit` after truncating.
    pub fn truncate(&mut self, len: usize) {
        unsafe {
            let (ptr, len_ptr, _) = self.triple_mut();
            while len < *len_ptr {
                let last_index = *len_ptr - 1;
                *len_ptr = last_index;
                ptr::drop_in_place(ptr.add(last_index));
            }
        }
    }

    /// Extracts a slice containing the entire vector.
    ///
    /// Equivalent to `&s[..]`.
    pub fn as_slice(&self) -> &[A::Item] {
        self
    }

    /// Extracts a mutable slice of the entire vector.
    ///
    /// Equivalent to `&mut s[..]`.
    pub fn as_mut_slice(&mut self) -> &mut [A::Item] {
        self
    }

    /// Remove the element at position `index`, replacing it with the last element.
    ///
    /// This does not preserve ordering, but is O(1).
    ///
    /// Panics if `index` is out of bounds.
    #[inline]
    pub fn swap_remove(&mut self, index: usize) -> A::Item {
        let len = self.len();
        self.swap(len - 1, index);
        self.pop()
            .unwrap_or_else(|| unsafe { unreachable_unchecked() })
    }

    /// Remove all elements from the vector.
    #[inline]
    pub fn clear(&mut self) {
        self.truncate(0);
    }

    /// Remove and return the element at position `index`, shifting all elements after it to the
    /// left.
    ///
    /// Panics if `index` is out of bounds.
    pub fn remove(&mut self, index: usize) -> A::Item {
        unsafe {
            let (mut ptr, len_ptr, _) = self.triple_mut();
            let len = *len_ptr;
            assert!(index < len);
            *len_ptr = len - 1;
            ptr = ptr.add(index);
            let item = ptr::read(ptr);
            ptr::copy(ptr.add(1), ptr, len - index - 1);
            item
        }
    }

    /// Insert an element at position `index`, shifting all elements after it to the right.
    ///
    /// Panics if `index` is out of bounds.
    pub fn insert(&mut self, index: usize, element: A::Item) {
        self.reserve(1);

        unsafe {
            let (mut ptr, len_ptr, _) = self.triple_mut();
            let len = *len_ptr;
            assert!(index <= len);
            *len_ptr = len + 1;
            ptr = ptr.add(index);
            ptr::copy(ptr, ptr.add(1), len - index);
            ptr::write(ptr, element);
        }
    }

    /// Insert multiple elements at position `index`, shifting all following elements toward the
    /// back.
    pub fn insert_many<I: IntoIterator<Item = A::Item>>(&mut self, index: usize, iterable: I) {
        let iter = iterable.into_iter();
        if index == self.len() {
            return self.extend(iter);
        }

        let (lower_size_bound, _) = iter.size_hint();
        assert!(lower_size_bound <= core::isize::MAX as usize); // Ensure offset is indexable
        assert!(index + lower_size_bound >= index); // Protect against overflow
        self.reserve(lower_size_bound);

        unsafe {
            let old_len = self.len();
            assert!(index <= old_len);
            let mut ptr = self.as_mut_ptr().add(index);

            // Move the trailing elements.
            ptr::copy(ptr, ptr.add(lower_size_bound), old_len - index);

            // In case the iterator panics, don't double-drop the items we just copied above.
            self.set_len(index);

            let mut num_added = 0;
            for element in iter {
                let mut cur = ptr.add(num_added);
                if num_added >= lower_size_bound {
                    // Iterator provided more elements than the hint.  Move trailing items again.
                    self.reserve(1);
                    ptr = self.as_mut_ptr().add(index);
                    cur = ptr.add(num_added);
                    ptr::copy(cur, cur.add(1), old_len - index);
                }
                ptr::write(cur, element);
                num_added += 1;
            }
            if num_added < lower_size_bound {
                // Iterator provided fewer elements than the hint
                ptr::copy(
                    ptr.add(lower_size_bound),
                    ptr.add(num_added),
                    old_len - index,
                );
            }

            self.set_len(old_len + num_added);
        }
    }

    /// Convert a SmallVec to a Vec, without reallocating if the SmallVec has already spilled onto
    /// the heap.
    pub fn into_vec(self) -> Vec<A::Item> {
        if self.spilled() {
            unsafe {
                let (ptr, len) = self.data.heap();
                let v = Vec::from_raw_parts(ptr, len, self.capacity);
                mem::forget(self);
                v
            }
        } else {
            self.into_iter().collect()
        }
    }

    /// Converts a `SmallVec` into a `Box<[T]>` without reallocating if the `SmallVec` has already spilled
    /// onto the heap.
    ///
    /// Note that this will drop any excess capacity.
    pub fn into_boxed_slice(self) -> Box<[A::Item]> {
        self.into_vec().into_boxed_slice()
    }

    /// Convert the SmallVec into an `A` if possible. Otherwise return `Err(Self)`.
    ///
    /// This method returns `Err(Self)` if the SmallVec is too short (and the `A` contains uninitialized elements),
    /// or if the SmallVec is too long (and all the elements were spilled to the heap).
    pub fn into_inner(self) -> Result<A, Self> {
        if self.spilled() || self.len() != A::size() {
            Err(self)
        } else {
            unsafe {
                let data = ptr::read(&self.data);
                mem::forget(self);
                Ok(data.into_inline().assume_init())
            }
        }
    }

    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&e)` returns `false`.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    pub fn retain<F: FnMut(&mut A::Item) -> bool>(&mut self, mut f: F) {
        let mut del = 0;
        let len = self.len();
        for i in 0..len {
            if !f(&mut self[i]) {
                del += 1;
            } else if del > 0 {
                self.swap(i - del, i);
            }
        }
        self.truncate(len - del);
    }

    /// Removes consecutive duplicate elements.
    pub fn dedup(&mut self)
    where
        A::Item: PartialEq<A::Item>,
    {
        self.dedup_by(|a, b| a == b);
    }

    /// Removes consecutive duplicate elements using the given equality relation.
    pub fn dedup_by<F>(&mut self, mut same_bucket: F)
    where
        F: FnMut(&mut A::Item, &mut A::Item) -> bool,
    {
        // See the implementation of Vec::dedup_by in the
        // standard library for an explanation of this algorithm.
        let len = self.len();
        if len <= 1 {
            return;
        }

        let ptr = self.as_mut_ptr();
        let mut w: usize = 1;

        unsafe {
            for r in 1..len {
                let p_r = ptr.add(r);
                let p_wm1 = ptr.add(w - 1);
                if !same_bucket(&mut *p_r, &mut *p_wm1) {
                    if r != w {
                        let p_w = p_wm1.add(1);
                        mem::swap(&mut *p_r, &mut *p_w);
                    }
                    w += 1;
                }
            }
        }

        self.truncate(w);
    }

    /// Removes consecutive elements that map to the same key.
    pub fn dedup_by_key<F, K>(&mut self, mut key: F)
    where
        F: FnMut(&mut A::Item) -> K,
        K: PartialEq<K>,
    {
        self.dedup_by(|a, b| key(a) == key(b));
    }

    /// Creates a `SmallVec` directly from the raw components of another
    /// `SmallVec`.
    ///
    /// # Safety
    ///
    /// This is highly unsafe, due to the number of invariants that aren't
    /// checked:
    ///
    /// * `ptr` needs to have been previously allocated via `SmallVec` for its
    ///   spilled storage (at least, it's highly likely to be incorrect if it
    ///   wasn't).
    /// * `ptr`'s `A::Item` type needs to be the same size and alignment that
    ///   it was allocated with
    /// * `length` needs to be less than or equal to `capacity`.
    /// * `capacity` needs to be the capacity that the pointer was allocated
    ///   with.
    ///
    /// Violating these may cause problems like corrupting the allocator's
    /// internal data structures.
    ///
    /// Additionally, `capacity` must be greater than the amount of inline
    /// storage `A` has; that is, the new `SmallVec` must need to spill over
    /// into heap allocated storage. This condition is asserted against.
    ///
    /// The ownership of `ptr` is effectively transferred to the
    /// `SmallVec` which may then deallocate, reallocate or change the
    /// contents of memory pointed to by the pointer at will. Ensure
    /// that nothing else uses the pointer after calling this
    /// function.
    ///
    /// # Examples
    ///
    /// ```
    /// # #[macro_use] extern crate smallvec;
    /// # use smallvec::SmallVec;
    /// use std::mem;
    /// use std::ptr;
    ///
    /// fn main() {
    ///     let mut v: SmallVec<[_; 1]> = smallvec![1, 2, 3];
    ///
    ///     // Pull out the important parts of `v`.
    ///     let p = v.as_mut_ptr();
    ///     let len = v.len();
    ///     let cap = v.capacity();
    ///     let spilled = v.spilled();
    ///
    ///     unsafe {
    ///         // Forget all about `v`. The heap allocation that stored the
    ///         // three values won't be deallocated.
    ///         mem::forget(v);
    ///
    ///         // Overwrite memory with [4, 5, 6].
    ///         //
    ///         // This is only safe if `spilled` is true! Otherwise, we are
    ///         // writing into the old `SmallVec`'s inline storage on the
    ///         // stack.
    ///         assert!(spilled);
    ///         for i in 0..len {
    ///             ptr::write(p.add(i), 4 + i);
    ///         }
    ///
    ///         // Put everything back together into a SmallVec with a different
    ///         // amount of inline storage, but which is still less than `cap`.
    ///         let rebuilt = SmallVec::<[_; 2]>::from_raw_parts(p, len, cap);
    ///         assert_eq!(&*rebuilt, &[4, 5, 6]);
    ///     }
    /// }
    #[inline]
    pub unsafe fn from_raw_parts(ptr: *mut A::Item, length: usize, capacity: usize) -> SmallVec<A> {
        assert!(capacity > A::size());
        SmallVec {
            capacity,
            data: SmallVecData::from_heap(ptr, length),
        }
    }
}

impl<A: Array> SmallVec<A>
where
    A::Item: Copy,
{
    /// Copy the elements from a slice into a new `SmallVec`.
    ///
    /// For slices of `Copy` types, this is more efficient than `SmallVec::from(slice)`.
    pub fn from_slice(slice: &[A::Item]) -> Self {
        let len = slice.len();
        if len <= A::size() {
            SmallVec {
                capacity: len,
                data: SmallVecData::from_inline(unsafe {
                    let mut data: MaybeUninit<A> = MaybeUninit::uninit();
                    ptr::copy_nonoverlapping(
                        slice.as_ptr(),
                        data.as_mut_ptr() as *mut A::Item,
                        len,
                    );
                    data
                }),
            }
        } else {
            let mut b = slice.to_vec();
            let (ptr, cap) = (b.as_mut_ptr(), b.capacity());
            mem::forget(b);
            SmallVec {
                capacity: cap,
                data: SmallVecData::from_heap(ptr, len),
            }
        }
    }

    /// Copy elements from a slice into the vector at position `index`, shifting any following
    /// elements toward the back.
    ///
    /// For slices of `Copy` types, this is more efficient than `insert`.
    pub fn insert_from_slice(&mut self, index: usize, slice: &[A::Item]) {
        self.reserve(slice.len());

        let len = self.len();
        assert!(index <= len);

        unsafe {
            let slice_ptr = slice.as_ptr();
            let ptr = self.as_mut_ptr().add(index);
            ptr::copy(ptr, ptr.add(slice.len()), len - index);
            ptr::copy_nonoverlapping(slice_ptr, ptr, slice.len());
            self.set_len(len + slice.len());
        }
    }

    /// Copy elements from a slice and append them to the vector.
    ///
    /// For slices of `Copy` types, this is more efficient than `extend`.
    #[inline]
    pub fn extend_from_slice(&mut self, slice: &[A::Item]) {
        let len = self.len();
        self.insert_from_slice(len, slice);
    }
}

impl<A: Array> SmallVec<A>
where
    A::Item: Clone,
{
    /// Resizes the vector so that its length is equal to `len`.
    ///
    /// If `len` is less than the current length, the vector simply truncated.
    ///
    /// If `len` is greater than the current length, `value` is appended to the
    /// vector until its length equals `len`.
    pub fn resize(&mut self, len: usize, value: A::Item) {
        let old_len = self.len();

        if len > old_len {
            self.extend(repeat(value).take(len - old_len));
        } else {
            self.truncate(len);
        }
    }

    /// Creates a `SmallVec` with `n` copies of `elem`.
    /// ```
    /// use smallvec::SmallVec;
    ///
    /// let v = SmallVec::<[char; 128]>::from_elem('d', 2);
    /// assert_eq!(v, SmallVec::from_buf(['d', 'd']));
    /// ```
    pub fn from_elem(elem: A::Item, n: usize) -> Self {
        if n > A::size() {
            vec![elem; n].into()
        } else {
            let mut v = SmallVec::<A>::new();
            unsafe {
                let (ptr, len_ptr, _) = v.triple_mut();
                let mut local_len = SetLenOnDrop::new(len_ptr);

                for i in 0..n {
                    ::core::ptr::write(ptr.add(i), elem.clone());
                    local_len.increment_len(1);
                }
            }
            v
        }
    }
}

impl<A: Array> ops::Deref for SmallVec<A> {
    type Target = [A::Item];
    #[inline]
    fn deref(&self) -> &[A::Item] {
        unsafe {
            let (ptr, len, _) = self.triple();
            slice::from_raw_parts(ptr, len)
        }
    }
}

impl<A: Array> ops::DerefMut for SmallVec<A> {
    #[inline]
    fn deref_mut(&mut self) -> &mut [A::Item] {
        unsafe {
            let (ptr, &mut len, _) = self.triple_mut();
            slice::from_raw_parts_mut(ptr, len)
        }
    }
}

impl<A: Array> AsRef<[A::Item]> for SmallVec<A> {
    #[inline]
    fn as_ref(&self) -> &[A::Item] {
        self
    }
}

impl<A: Array> AsMut<[A::Item]> for SmallVec<A> {
    #[inline]
    fn as_mut(&mut self) -> &mut [A::Item] {
        self
    }
}

impl<A: Array> Borrow<[A::Item]> for SmallVec<A> {
    #[inline]
    fn borrow(&self) -> &[A::Item] {
        self
    }
}

impl<A: Array> BorrowMut<[A::Item]> for SmallVec<A> {
    #[inline]
    fn borrow_mut(&mut self) -> &mut [A::Item] {
        self
    }
}

#[cfg(feature = "write")]
impl<A: Array<Item = u8>> io::Write for SmallVec<A> {
    #[inline]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.extend_from_slice(buf);
        Ok(buf.len())
    }

    #[inline]
    fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        self.extend_from_slice(buf);
        Ok(())
    }

    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[cfg(feature = "serde")]
impl<A: Array> Serialize for SmallVec<A>
where
    A::Item: Serialize,
{
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let mut state = serializer.serialize_seq(Some(self.len()))?;
        for item in self {
            state.serialize_element(&item)?;
        }
        state.end()
    }
}

#[cfg(feature = "serde")]
impl<'de, A: Array> Deserialize<'de> for SmallVec<A>
where
    A::Item: Deserialize<'de>,
{
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_seq(SmallVecVisitor {
            phantom: PhantomData,
        })
    }
}

#[cfg(feature = "serde")]
struct SmallVecVisitor<A> {
    phantom: PhantomData<A>,
}

#[cfg(feature = "serde")]
impl<'de, A: Array> Visitor<'de> for SmallVecVisitor<A>
where
    A::Item: Deserialize<'de>,
{
    type Value = SmallVec<A>;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str("a sequence")
    }

    fn visit_seq<B>(self, mut seq: B) -> Result<Self::Value, B::Error>
    where
        B: SeqAccess<'de>,
    {
        let len = seq.size_hint().unwrap_or(0);
        let mut values = SmallVec::with_capacity(len);

        while let Some(value) = seq.next_element()? {
            values.push(value);
        }

        Ok(values)
    }
}

#[cfg(feature = "specialization")]
trait SpecFrom<A: Array, S> {
    fn spec_from(slice: S) -> SmallVec<A>;
}

#[cfg(feature = "specialization")]
mod specialization;

#[cfg(feature = "specialization")]
impl<'a, A: Array> SpecFrom<A, &'a [A::Item]> for SmallVec<A>
where
    A::Item: Copy,
{
    #[inline]
    fn spec_from(slice: &'a [A::Item]) -> SmallVec<A> {
        SmallVec::from_slice(slice)
    }
}

impl<'a, A: Array> From<&'a [A::Item]> for SmallVec<A>
where
    A::Item: Clone,
{
    #[cfg(not(feature = "specialization"))]
    #[inline]
    fn from(slice: &'a [A::Item]) -> SmallVec<A> {
        slice.iter().cloned().collect()
    }

    #[cfg(feature = "specialization")]
    #[inline]
    fn from(slice: &'a [A::Item]) -> SmallVec<A> {
        SmallVec::spec_from(slice)
    }
}

impl<A: Array> From<Vec<A::Item>> for SmallVec<A> {
    #[inline]
    fn from(vec: Vec<A::Item>) -> SmallVec<A> {
        SmallVec::from_vec(vec)
    }
}

impl<A: Array> From<A> for SmallVec<A> {
    #[inline]
    fn from(array: A) -> SmallVec<A> {
        SmallVec::from_buf(array)
    }
}

impl<A: Array, I: SliceIndex<[A::Item]>> ops::Index<I> for SmallVec<A> {
    type Output = I::Output;

    fn index(&self, index: I) -> &I::Output {
        &(**self)[index]
    }
}

impl<A: Array, I: SliceIndex<[A::Item]>> ops::IndexMut<I> for SmallVec<A> {
    fn index_mut(&mut self, index: I) -> &mut I::Output {
        &mut (&mut **self)[index]
    }
}

impl<A: Array> ExtendFromSlice<A::Item> for SmallVec<A>
where
    A::Item: Copy,
{
    fn extend_from_slice(&mut self, other: &[A::Item]) {
        SmallVec::extend_from_slice(self, other)
    }
}

impl<A: Array> FromIterator<A::Item> for SmallVec<A> {
    #[inline]
    fn from_iter<I: IntoIterator<Item = A::Item>>(iterable: I) -> SmallVec<A> {
        let mut v = SmallVec::new();
        v.extend(iterable);
        v
    }
}

impl<A: Array> Extend<A::Item> for SmallVec<A> {
    fn extend<I: IntoIterator<Item = A::Item>>(&mut self, iterable: I) {
        let mut iter = iterable.into_iter();
        let (lower_size_bound, _) = iter.size_hint();
        self.reserve(lower_size_bound);

        unsafe {
            let (ptr, len_ptr, cap) = self.triple_mut();
            let mut len = SetLenOnDrop::new(len_ptr);
            while len.get() < cap {
                if let Some(out) = iter.next() {
                    ptr::write(ptr.add(len.get()), out);
                    len.increment_len(1);
                } else {
                    return;
                }
            }
        }

        for elem in iter {
            self.push(elem);
        }
    }
}

impl<A: Array> fmt::Debug for SmallVec<A>
where
    A::Item: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_list().entries(self.iter()).finish()
    }
}

impl<A: Array> Default for SmallVec<A> {
    #[inline]
    fn default() -> SmallVec<A> {
        SmallVec::new()
    }
}

#[cfg(feature = "may_dangle")]
unsafe impl<#[may_dangle] A: Array> Drop for SmallVec<A> {
    fn drop(&mut self) {
        unsafe {
            if self.spilled() {
                let (ptr, len) = self.data.heap();
                Vec::from_raw_parts(ptr, len, self.capacity);
            } else {
                ptr::drop_in_place(&mut self[..]);
            }
        }
    }
}

#[cfg(not(feature = "may_dangle"))]
impl<A: Array> Drop for SmallVec<A> {
    fn drop(&mut self) {
        unsafe {
            if self.spilled() {
                let (ptr, len) = self.data.heap();
                Vec::from_raw_parts(ptr, len, self.capacity);
            } else {
                ptr::drop_in_place(&mut self[..]);
            }
        }
    }
}

impl<A: Array> Clone for SmallVec<A>
where
    A::Item: Clone,
{
    #[inline]
    fn clone(&self) -> SmallVec<A> {
        let mut new_vector = SmallVec::with_capacity(self.len());
        for element in self.iter() {
            new_vector.push((*element).clone())
        }
        new_vector
    }
}

impl<A: Array, B: Array> PartialEq<SmallVec<B>> for SmallVec<A>
where
    A::Item: PartialEq<B::Item>,
{
    #[inline]
    fn eq(&self, other: &SmallVec<B>) -> bool {
        self[..] == other[..]
    }
}

impl<A: Array> Eq for SmallVec<A> where A::Item: Eq {}

impl<A: Array> PartialOrd for SmallVec<A>
where
    A::Item: PartialOrd,
{
    #[inline]
    fn partial_cmp(&self, other: &SmallVec<A>) -> Option<cmp::Ordering> {
        PartialOrd::partial_cmp(&**self, &**other)
    }
}

impl<A: Array> Ord for SmallVec<A>
where
    A::Item: Ord,
{
    #[inline]
    fn cmp(&self, other: &SmallVec<A>) -> cmp::Ordering {
        Ord::cmp(&**self, &**other)
    }
}

impl<A: Array> Hash for SmallVec<A>
where
    A::Item: Hash,
{
    fn hash<H: Hasher>(&self, state: &mut H) {
        (**self).hash(state)
    }
}

unsafe impl<A: Array> Send for SmallVec<A> where A::Item: Send {}

/// An iterator that consumes a `SmallVec` and yields its items by value.
///
/// Returned from [`SmallVec::into_iter`][1].
///
/// [1]: struct.SmallVec.html#method.into_iter
pub struct IntoIter<A: Array> {
    data: SmallVec<A>,
    current: usize,
    end: usize,
}

impl<A: Array> fmt::Debug for IntoIter<A>
where
    A::Item: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("IntoIter").field(&self.as_slice()).finish()
    }
}

impl<A: Array + Clone> Clone for IntoIter<A>
where
    A::Item: Clone,
{
    fn clone(&self) -> IntoIter<A> {
        SmallVec::from(self.as_slice()).into_iter()
    }
}

impl<A: Array> Drop for IntoIter<A> {
    fn drop(&mut self) {
        for _ in self {}
    }
}

impl<A: Array> Iterator for IntoIter<A> {
    type Item = A::Item;

    #[inline]
    fn next(&mut self) -> Option<A::Item> {
        if self.current == self.end {
            None
        } else {
            unsafe {
                let current = self.current;
                self.current += 1;
                Some(ptr::read(self.data.as_ptr().add(current)))
            }
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let size = self.end - self.current;
        (size, Some(size))
    }
}

impl<A: Array> DoubleEndedIterator for IntoIter<A> {
    #[inline]
    fn next_back(&mut self) -> Option<A::Item> {
        if self.current == self.end {
            None
        } else {
            unsafe {
                self.end -= 1;
                Some(ptr::read(self.data.as_ptr().add(self.end)))
            }
        }
    }
}

impl<A: Array> ExactSizeIterator for IntoIter<A> {}
impl<A: Array> FusedIterator for IntoIter<A> {}

impl<A: Array> IntoIter<A> {
    /// Returns the remaining items of this iterator as a slice.
    pub fn as_slice(&self) -> &[A::Item] {
        let len = self.end - self.current;
        unsafe { core::slice::from_raw_parts(self.data.as_ptr().add(self.current), len) }
    }

    /// Returns the remaining items of this iterator as a mutable slice.
    pub fn as_mut_slice(&mut self) -> &mut [A::Item] {
        let len = self.end - self.current;
        unsafe { core::slice::from_raw_parts_mut(self.data.as_mut_ptr().add(self.current), len) }
    }
}

impl<A: Array> IntoIterator for SmallVec<A> {
    type IntoIter = IntoIter<A>;
    type Item = A::Item;
    fn into_iter(mut self) -> Self::IntoIter {
        unsafe {
            // Set SmallVec len to zero as `IntoIter` drop handles dropping of the elements
            let len = self.len();
            self.set_len(0);
            IntoIter {
                data: self,
                current: 0,
                end: len,
            }
        }
    }
}

impl<'a, A: Array> IntoIterator for &'a SmallVec<A> {
    type IntoIter = slice::Iter<'a, A::Item>;
    type Item = &'a A::Item;
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a, A: Array> IntoIterator for &'a mut SmallVec<A> {
    type IntoIter = slice::IterMut<'a, A::Item>;
    type Item = &'a mut A::Item;
    fn into_iter(self) -> Self::IntoIter {
        self.iter_mut()
    }
}

/// Types that can be used as the backing store for a SmallVec
pub unsafe trait Array {
    /// The type of the array's elements.
    type Item;
    /// Returns the number of items the array can hold.
    fn size() -> usize;
}

/// Set the length of the vec when the `SetLenOnDrop` value goes out of scope.
///
/// Copied from https://github.com/rust-lang/rust/pull/36355
struct SetLenOnDrop<'a> {
    len: &'a mut usize,
    local_len: usize,
}

impl<'a> SetLenOnDrop<'a> {
    #[inline]
    fn new(len: &'a mut usize) -> Self {
        SetLenOnDrop {
            local_len: *len,
            len,
        }
    }

    #[inline]
    fn get(&self) -> usize {
        self.local_len
    }

    #[inline]
    fn increment_len(&mut self, increment: usize) {
        self.local_len += increment;
    }
}

impl<'a> Drop for SetLenOnDrop<'a> {
    #[inline]
    fn drop(&mut self) {
        *self.len = self.local_len;
    }
}

#[cfg(feature = "const_generics")]
unsafe impl<T, const N: usize> Array for [T; N] {
    type Item = T;
    fn size() -> usize { N }
}

#[cfg(not(feature = "const_generics"))]
macro_rules! impl_array(
    ($($size:expr),+) => {
        $(
            unsafe impl<T> Array for [T; $size] {
                type Item = T;
                fn size() -> usize { $size }
            }
        )+
    }
);

#[cfg(not(feature = "const_generics"))]
impl_array!(
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 24, 32, 36, 0x40, 0x60, 0x80,
    0x100, 0x200, 0x400, 0x600, 0x800, 0x1000, 0x2000, 0x4000, 0x6000, 0x8000, 0x10000, 0x20000,
    0x40000, 0x60000, 0x80000, 0x10_0000
);

/// Convenience trait for constructing a `SmallVec`
pub trait ToSmallVec<A:Array> {
    /// Construct a new `SmallVec` from a slice.
    fn to_smallvec(&self) -> SmallVec<A>;
}

impl<A:Array> ToSmallVec<A> for [A::Item]
    where A::Item: Copy {
    #[inline]
    fn to_smallvec(&self) -> SmallVec<A> {
        SmallVec::from_slice(self)
    }
}

#[cfg(test)]
mod tests {
    use crate::SmallVec;

    use std::iter::FromIterator;

    use alloc::borrow::ToOwned;
    use alloc::boxed::Box;
    use alloc::rc::Rc;
    use alloc::{vec, vec::Vec};

    #[test]
    pub fn test_zero() {
        let mut v = SmallVec::<[_; 0]>::new();
        assert!(!v.spilled());
        v.push(0usize);
        assert!(v.spilled());
        assert_eq!(&*v, &[0]);
    }

    // We heap allocate all these strings so that double frees will show up under valgrind.

    #[test]
    pub fn test_inline() {
        let mut v = SmallVec::<[_; 16]>::new();
        v.push("hello".to_owned());
        v.push("there".to_owned());
        assert_eq!(&*v, &["hello".to_owned(), "there".to_owned(),][..]);
    }

    #[test]
    pub fn test_spill() {
        let mut v = SmallVec::<[_; 2]>::new();
        v.push("hello".to_owned());
        assert_eq!(v[0], "hello");
        v.push("there".to_owned());
        v.push("burma".to_owned());
        assert_eq!(v[0], "hello");
        v.push("shave".to_owned());
        assert_eq!(
            &*v,
            &[
                "hello".to_owned(),
                "there".to_owned(),
                "burma".to_owned(),
                "shave".to_owned(),
            ][..]
        );
    }

    #[test]
    pub fn test_double_spill() {
        let mut v = SmallVec::<[_; 2]>::new();
        v.push("hello".to_owned());
        v.push("there".to_owned());
        v.push("burma".to_owned());
        v.push("shave".to_owned());
        v.push("hello".to_owned());
        v.push("there".to_owned());
        v.push("burma".to_owned());
        v.push("shave".to_owned());
        assert_eq!(
            &*v,
            &[
                "hello".to_owned(),
                "there".to_owned(),
                "burma".to_owned(),
                "shave".to_owned(),
                "hello".to_owned(),
                "there".to_owned(),
                "burma".to_owned(),
                "shave".to_owned(),
            ][..]
        );
    }

    /// https://github.com/servo/rust-smallvec/issues/4
    #[test]
    fn issue_4() {
        SmallVec::<[Box<u32>; 2]>::new();
    }

    /// https://github.com/servo/rust-smallvec/issues/5
    #[test]
    fn issue_5() {
        assert!(Some(SmallVec::<[&u32; 2]>::new()).is_some());
    }

    #[test]
    fn test_with_capacity() {
        let v: SmallVec<[u8; 3]> = SmallVec::with_capacity(1);
        assert!(v.is_empty());
        assert!(!v.spilled());
        assert_eq!(v.capacity(), 3);

        let v: SmallVec<[u8; 3]> = SmallVec::with_capacity(10);
        assert!(v.is_empty());
        assert!(v.spilled());
        assert_eq!(v.capacity(), 10);
    }

    #[test]
    fn drain() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        assert_eq!(v.drain(..).collect::<Vec<_>>(), &[3]);

        // spilling the vec
        v.push(3);
        v.push(4);
        v.push(5);
        let old_capacity = v.capacity();
        assert_eq!(v.drain(1..).collect::<Vec<_>>(), &[4, 5]);
        // drain should not change the capacity
        assert_eq!(v.capacity(), old_capacity);
    }

    #[test]
    fn drain_rev() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        assert_eq!(v.drain(..).rev().collect::<Vec<_>>(), &[3]);

        // spilling the vec
        v.push(3);
        v.push(4);
        v.push(5);
        assert_eq!(v.drain(..).rev().collect::<Vec<_>>(), &[5, 4, 3]);
    }

    #[test]
    fn drain_forget() {
        let mut v: SmallVec<[u8; 1]> = smallvec![0, 1, 2, 3, 4, 5, 6, 7];
        std::mem::forget(v.drain(2..5));
        assert_eq!(v.len(), 2);
    }

    #[test]
    fn into_iter() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        assert_eq!(v.into_iter().collect::<Vec<_>>(), &[3]);

        // spilling the vec
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        v.push(4);
        v.push(5);
        assert_eq!(v.into_iter().collect::<Vec<_>>(), &[3, 4, 5]);
    }

    #[test]
    fn into_iter_rev() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        assert_eq!(v.into_iter().rev().collect::<Vec<_>>(), &[3]);

        // spilling the vec
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(3);
        v.push(4);
        v.push(5);
        assert_eq!(v.into_iter().rev().collect::<Vec<_>>(), &[5, 4, 3]);
    }

    #[test]
    fn into_iter_drop() {
        use std::cell::Cell;

        struct DropCounter<'a>(&'a Cell<i32>);

        impl<'a> Drop for DropCounter<'a> {
            fn drop(&mut self) {
                self.0.set(self.0.get() + 1);
            }
        }

        {
            let cell = Cell::new(0);
            let mut v: SmallVec<[DropCounter<'_>; 2]> = SmallVec::new();
            v.push(DropCounter(&cell));
            v.into_iter();
            assert_eq!(cell.get(), 1);
        }

        {
            let cell = Cell::new(0);
            let mut v: SmallVec<[DropCounter<'_>; 2]> = SmallVec::new();
            v.push(DropCounter(&cell));
            v.push(DropCounter(&cell));
            assert!(v.into_iter().next().is_some());
            assert_eq!(cell.get(), 2);
        }

        {
            let cell = Cell::new(0);
            let mut v: SmallVec<[DropCounter<'_>; 2]> = SmallVec::new();
            v.push(DropCounter(&cell));
            v.push(DropCounter(&cell));
            v.push(DropCounter(&cell));
            assert!(v.into_iter().next().is_some());
            assert_eq!(cell.get(), 3);
        }
        {
            let cell = Cell::new(0);
            let mut v: SmallVec<[DropCounter<'_>; 2]> = SmallVec::new();
            v.push(DropCounter(&cell));
            v.push(DropCounter(&cell));
            v.push(DropCounter(&cell));
            {
                let mut it = v.into_iter();
                assert!(it.next().is_some());
                assert!(it.next_back().is_some());
            }
            assert_eq!(cell.get(), 3);
        }
    }

    #[test]
    fn test_capacity() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.reserve(1);
        assert_eq!(v.capacity(), 2);
        assert!(!v.spilled());

        v.reserve_exact(0x100);
        assert!(v.capacity() >= 0x100);

        v.push(0);
        v.push(1);
        v.push(2);
        v.push(3);

        v.shrink_to_fit();
        assert!(v.capacity() < 0x100);
    }

    #[test]
    fn test_truncate() {
        let mut v: SmallVec<[Box<u8>; 8]> = SmallVec::new();

        for x in 0..8 {
            v.push(Box::new(x));
        }
        v.truncate(4);

        assert_eq!(v.len(), 4);
        assert!(!v.spilled());

        assert_eq!(*v.swap_remove(1), 1);
        assert_eq!(*v.remove(1), 3);
        v.insert(1, Box::new(3));

        assert_eq!(&v.iter().map(|v| **v).collect::<Vec<_>>(), &[0, 3, 2]);
    }

    #[test]
    fn test_insert_many() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        for x in 0..4 {
            v.push(x);
        }
        assert_eq!(v.len(), 4);
        v.insert_many(1, [5, 6].iter().cloned());
        assert_eq!(
            &v.iter().map(|v| *v).collect::<Vec<_>>(),
            &[0, 5, 6, 1, 2, 3]
        );
    }

    struct MockHintIter<T: Iterator> {
        x: T,
        hint: usize,
    }
    impl<T: Iterator> Iterator for MockHintIter<T> {
        type Item = T::Item;
        fn next(&mut self) -> Option<Self::Item> {
            self.x.next()
        }
        fn size_hint(&self) -> (usize, Option<usize>) {
            (self.hint, None)
        }
    }

    #[test]
    fn test_insert_many_short_hint() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        for x in 0..4 {
            v.push(x);
        }
        assert_eq!(v.len(), 4);
        v.insert_many(
            1,
            MockHintIter {
                x: [5, 6].iter().cloned(),
                hint: 5,
            },
        );
        assert_eq!(
            &v.iter().map(|v| *v).collect::<Vec<_>>(),
            &[0, 5, 6, 1, 2, 3]
        );
    }

    #[test]
    fn test_insert_many_long_hint() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        for x in 0..4 {
            v.push(x);
        }
        assert_eq!(v.len(), 4);
        v.insert_many(
            1,
            MockHintIter {
                x: [5, 6].iter().cloned(),
                hint: 1,
            },
        );
        assert_eq!(
            &v.iter().map(|v| *v).collect::<Vec<_>>(),
            &[0, 5, 6, 1, 2, 3]
        );
    }

    #[test]
    // https://github.com/servo/rust-smallvec/issues/96
    fn test_insert_many_panic() {
        struct PanicOnDoubleDrop {
            dropped: Box<bool>,
        }

        impl Drop for PanicOnDoubleDrop {
            fn drop(&mut self) {
                assert!(!*self.dropped, "already dropped");
                *self.dropped = true;
            }
        }

        struct BadIter;
        impl Iterator for BadIter {
            type Item = PanicOnDoubleDrop;
            fn size_hint(&self) -> (usize, Option<usize>) {
                (1, None)
            }
            fn next(&mut self) -> Option<Self::Item> {
                panic!()
            }
        }

        let mut vec: SmallVec<[PanicOnDoubleDrop; 0]> = vec![
            PanicOnDoubleDrop {
                dropped: Box::new(false),
            },
            PanicOnDoubleDrop {
                dropped: Box::new(false),
            },
        ]
        .into();
        let result = ::std::panic::catch_unwind(move || {
            vec.insert_many(0, BadIter);
        });
        assert!(result.is_err());
    }

    #[test]
    #[should_panic]
    fn test_invalid_grow() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        v.extend(0..8);
        v.grow(5);
    }

    #[test]
    fn test_insert_from_slice() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        for x in 0..4 {
            v.push(x);
        }
        assert_eq!(v.len(), 4);
        v.insert_from_slice(1, &[5, 6]);
        assert_eq!(
            &v.iter().map(|v| *v).collect::<Vec<_>>(),
            &[0, 5, 6, 1, 2, 3]
        );
    }

    #[test]
    fn test_extend_from_slice() {
        let mut v: SmallVec<[u8; 8]> = SmallVec::new();
        for x in 0..4 {
            v.push(x);
        }
        assert_eq!(v.len(), 4);
        v.extend_from_slice(&[5, 6]);
        assert_eq!(
            &v.iter().map(|v| *v).collect::<Vec<_>>(),
            &[0, 1, 2, 3, 5, 6]
        );
    }

    #[test]
    #[should_panic]
    fn test_drop_panic_smallvec() {
        // This test should only panic once, and not double panic,
        // which would mean a double drop
        struct DropPanic;

        impl Drop for DropPanic {
            fn drop(&mut self) {
                panic!("drop");
            }
        }

        let mut v = SmallVec::<[_; 1]>::new();
        v.push(DropPanic);
    }

    #[test]
    fn test_eq() {
        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        let mut b: SmallVec<[u32; 2]> = SmallVec::new();
        let mut c: SmallVec<[u32; 2]> = SmallVec::new();
        // a = [1, 2]
        a.push(1);
        a.push(2);
        // b = [1, 2]
        b.push(1);
        b.push(2);
        // c = [3, 4]
        c.push(3);
        c.push(4);

        assert!(a == b);
        assert!(a != c);
    }

    #[test]
    fn test_ord() {
        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        let mut b: SmallVec<[u32; 2]> = SmallVec::new();
        let mut c: SmallVec<[u32; 2]> = SmallVec::new();
        // a = [1]
        a.push(1);
        // b = [1, 1]
        b.push(1);
        b.push(1);
        // c = [1, 2]
        c.push(1);
        c.push(2);

        assert!(a < b);
        assert!(b > a);
        assert!(b < c);
        assert!(c > b);
    }

    #[test]
    fn test_hash() {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::Hash;

        {
            let mut a: SmallVec<[u32; 2]> = SmallVec::new();
            let b = [1, 2];
            a.extend(b.iter().cloned());
            let mut hasher = DefaultHasher::new();
            assert_eq!(a.hash(&mut hasher), b.hash(&mut hasher));
        }
        {
            let mut a: SmallVec<[u32; 2]> = SmallVec::new();
            let b = [1, 2, 11, 12];
            a.extend(b.iter().cloned());
            let mut hasher = DefaultHasher::new();
            assert_eq!(a.hash(&mut hasher), b.hash(&mut hasher));
        }
    }

    #[test]
    fn test_as_ref() {
        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        a.push(1);
        assert_eq!(a.as_ref(), [1]);
        a.push(2);
        assert_eq!(a.as_ref(), [1, 2]);
        a.push(3);
        assert_eq!(a.as_ref(), [1, 2, 3]);
    }

    #[test]
    fn test_as_mut() {
        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        a.push(1);
        assert_eq!(a.as_mut(), [1]);
        a.push(2);
        assert_eq!(a.as_mut(), [1, 2]);
        a.push(3);
        assert_eq!(a.as_mut(), [1, 2, 3]);
        a.as_mut()[1] = 4;
        assert_eq!(a.as_mut(), [1, 4, 3]);
    }

    #[test]
    fn test_borrow() {
        use std::borrow::Borrow;

        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        a.push(1);
        assert_eq!(a.borrow(), [1]);
        a.push(2);
        assert_eq!(a.borrow(), [1, 2]);
        a.push(3);
        assert_eq!(a.borrow(), [1, 2, 3]);
    }

    #[test]
    fn test_borrow_mut() {
        use std::borrow::BorrowMut;

        let mut a: SmallVec<[u32; 2]> = SmallVec::new();
        a.push(1);
        assert_eq!(a.borrow_mut(), [1]);
        a.push(2);
        assert_eq!(a.borrow_mut(), [1, 2]);
        a.push(3);
        assert_eq!(a.borrow_mut(), [1, 2, 3]);
        BorrowMut::<[u32]>::borrow_mut(&mut a)[1] = 4;
        assert_eq!(a.borrow_mut(), [1, 4, 3]);
    }

    #[test]
    fn test_from() {
        assert_eq!(&SmallVec::<[u32; 2]>::from(&[1][..])[..], [1]);
        assert_eq!(&SmallVec::<[u32; 2]>::from(&[1, 2, 3][..])[..], [1, 2, 3]);

        let vec = vec![];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from(vec);
        assert_eq!(&*small_vec, &[]);
        drop(small_vec);

        let vec = vec![1, 2, 3, 4, 5];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from(vec);
        assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
        drop(small_vec);

        let vec = vec![1, 2, 3, 4, 5];
        let small_vec: SmallVec<[u8; 1]> = SmallVec::from(vec);
        assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
        drop(small_vec);

        let array = [1];
        let small_vec: SmallVec<[u8; 1]> = SmallVec::from(array);
        assert_eq!(&*small_vec, &[1]);
        drop(small_vec);

        let array = [99; 128];
        let small_vec: SmallVec<[u8; 128]> = SmallVec::from(array);
        assert_eq!(&*small_vec, vec![99u8; 128].as_slice());
        drop(small_vec);
    }

    #[test]
    fn test_from_slice() {
        assert_eq!(&SmallVec::<[u32; 2]>::from_slice(&[1][..])[..], [1]);
        assert_eq!(
            &SmallVec::<[u32; 2]>::from_slice(&[1, 2, 3][..])[..],
            [1, 2, 3]
        );
    }

    #[test]
    fn test_exact_size_iterator() {
        let mut vec = SmallVec::<[u32; 2]>::from(&[1, 2, 3][..]);
        assert_eq!(vec.clone().into_iter().len(), 3);
        assert_eq!(vec.drain(..2).len(), 2);
        assert_eq!(vec.into_iter().len(), 1);
    }

    #[test]
    fn test_into_iter_as_slice() {
        let vec = SmallVec::<[u32; 2]>::from(&[1, 2, 3][..]);
        let mut iter = vec.clone().into_iter();
        assert_eq!(iter.as_slice(), &[1, 2, 3]);
        assert_eq!(iter.as_mut_slice(), &[1, 2, 3]);
        iter.next();
        assert_eq!(iter.as_slice(), &[2, 3]);
        assert_eq!(iter.as_mut_slice(), &[2, 3]);
        iter.next_back();
        assert_eq!(iter.as_slice(), &[2]);
        assert_eq!(iter.as_mut_slice(), &[2]);
    }

    #[test]
    fn test_into_iter_clone() {
        // Test that the cloned iterator yields identical elements and that it owns its own copy
        // (i.e. no use after move errors).
        let mut iter = SmallVec::<[u8; 2]>::from_iter(0..3).into_iter();
        let mut clone_iter = iter.clone();
        while let Some(x) = iter.next() {
            assert_eq!(x, clone_iter.next().unwrap());
        }
        assert_eq!(clone_iter.next(), None);
    }

    #[test]
    fn test_into_iter_clone_partially_consumed_iterator() {
        // Test that the cloned iterator only contains the remaining elements of the original iterator.
        let mut iter = SmallVec::<[u8; 2]>::from_iter(0..3).into_iter().skip(1);
        let mut clone_iter = iter.clone();
        while let Some(x) = iter.next() {
            assert_eq!(x, clone_iter.next().unwrap());
        }
        assert_eq!(clone_iter.next(), None);
    }

    #[test]
    fn test_into_iter_clone_empty_smallvec() {
        let mut iter = SmallVec::<[u8; 2]>::new().into_iter();
        let mut clone_iter = iter.clone();
        assert_eq!(iter.next(), None);
        assert_eq!(clone_iter.next(), None);
    }

    #[test]
    fn shrink_to_fit_unspill() {
        let mut vec = SmallVec::<[u8; 2]>::from_iter(0..3);
        vec.pop();
        assert!(vec.spilled());
        vec.shrink_to_fit();
        assert!(!vec.spilled(), "shrink_to_fit will un-spill if possible");
    }

    #[test]
    fn test_into_vec() {
        let vec = SmallVec::<[u8; 2]>::from_iter(0..2);
        assert_eq!(vec.into_vec(), vec![0, 1]);

        let vec = SmallVec::<[u8; 2]>::from_iter(0..3);
        assert_eq!(vec.into_vec(), vec![0, 1, 2]);
    }

    #[test]
    fn test_into_inner() {
        let vec = SmallVec::<[u8; 2]>::from_iter(0..2);
        assert_eq!(vec.into_inner(), Ok([0, 1]));

        let vec = SmallVec::<[u8; 2]>::from_iter(0..1);
        assert_eq!(vec.clone().into_inner(), Err(vec));

        let vec = SmallVec::<[u8; 2]>::from_iter(0..3);
        assert_eq!(vec.clone().into_inner(), Err(vec));
    }

    #[test]
    fn test_from_vec() {
        let vec = vec![];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[]);
        drop(small_vec);

        let vec = vec![];
        let small_vec: SmallVec<[u8; 1]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[]);
        drop(small_vec);

        let vec = vec![1];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[1]);
        drop(small_vec);

        let vec = vec![1, 2, 3];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[1, 2, 3]);
        drop(small_vec);

        let vec = vec![1, 2, 3, 4, 5];
        let small_vec: SmallVec<[u8; 3]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
        drop(small_vec);

        let vec = vec![1, 2, 3, 4, 5];
        let small_vec: SmallVec<[u8; 1]> = SmallVec::from_vec(vec);
        assert_eq!(&*small_vec, &[1, 2, 3, 4, 5]);
        drop(small_vec);
    }

    #[test]
    fn test_retain() {
        // Test inline data storate
        let mut sv: SmallVec<[i32; 5]> = SmallVec::from_slice(&[1, 2, 3, 3, 4]);
        sv.retain(|&mut i| i != 3);
        assert_eq!(sv.pop(), Some(4));
        assert_eq!(sv.pop(), Some(2));
        assert_eq!(sv.pop(), Some(1));
        assert_eq!(sv.pop(), None);

        // Test spilled data storage
        let mut sv: SmallVec<[i32; 3]> = SmallVec::from_slice(&[1, 2, 3, 3, 4]);
        sv.retain(|&mut i| i != 3);
        assert_eq!(sv.pop(), Some(4));
        assert_eq!(sv.pop(), Some(2));
        assert_eq!(sv.pop(), Some(1));
        assert_eq!(sv.pop(), None);

        // Test that drop implementations are called for inline.
        let one = Rc::new(1);
        let mut sv: SmallVec<[Rc<i32>; 3]> = SmallVec::new();
        sv.push(Rc::clone(&one));
        assert_eq!(Rc::strong_count(&one), 2);
        sv.retain(|_| false);
        assert_eq!(Rc::strong_count(&one), 1);

        // Test that drop implementations are called for spilled data.
        let mut sv: SmallVec<[Rc<i32>; 1]> = SmallVec::new();
        sv.push(Rc::clone(&one));
        sv.push(Rc::new(2));
        assert_eq!(Rc::strong_count(&one), 2);
        sv.retain(|_| false);
        assert_eq!(Rc::strong_count(&one), 1);
    }

    #[test]
    fn test_dedup() {
        let mut dupes: SmallVec<[i32; 5]> = SmallVec::from_slice(&[1, 1, 2, 3, 3]);
        dupes.dedup();
        assert_eq!(&*dupes, &[1, 2, 3]);

        let mut empty: SmallVec<[i32; 5]> = SmallVec::new();
        empty.dedup();
        assert!(empty.is_empty());

        let mut all_ones: SmallVec<[i32; 5]> = SmallVec::from_slice(&[1, 1, 1, 1, 1]);
        all_ones.dedup();
        assert_eq!(all_ones.len(), 1);

        let mut no_dupes: SmallVec<[i32; 5]> = SmallVec::from_slice(&[1, 2, 3, 4, 5]);
        no_dupes.dedup();
        assert_eq!(no_dupes.len(), 5);
    }

    #[test]
    fn test_resize() {
        let mut v: SmallVec<[i32; 8]> = SmallVec::new();
        v.push(1);
        v.resize(5, 0);
        assert_eq!(v[..], [1, 0, 0, 0, 0][..]);

        v.resize(2, -1);
        assert_eq!(v[..], [1, 0][..]);
    }

    #[cfg(feature = "write")]
    #[test]
    fn test_write() {
        use std::io::Write;

        let data = [1, 2, 3, 4, 5];

        let mut small_vec: SmallVec<[u8; 2]> = SmallVec::new();
        let len = small_vec.write(&data[..]).unwrap();
        assert_eq!(len, 5);
        assert_eq!(small_vec.as_ref(), data.as_ref());

        let mut small_vec: SmallVec<[u8; 2]> = SmallVec::new();
        small_vec.write_all(&data[..]).unwrap();
        assert_eq!(small_vec.as_ref(), data.as_ref());
    }

    #[cfg(feature = "serde")]
    extern crate bincode;

    #[cfg(feature = "serde")]
    #[test]
    fn test_serde() {
        use self::bincode::{config, deserialize};
        let mut small_vec: SmallVec<[i32; 2]> = SmallVec::new();
        small_vec.push(1);
        let encoded = config().limit(100).serialize(&small_vec).unwrap();
        let decoded: SmallVec<[i32; 2]> = deserialize(&encoded).unwrap();
        assert_eq!(small_vec, decoded);
        small_vec.push(2);
        // Spill the vec
        small_vec.push(3);
        small_vec.push(4);
        // Check again after spilling.
        let encoded = config().limit(100).serialize(&small_vec).unwrap();
        let decoded: SmallVec<[i32; 2]> = deserialize(&encoded).unwrap();
        assert_eq!(small_vec, decoded);
    }

    #[test]
    fn grow_to_shrink() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(1);
        v.push(2);
        v.push(3);
        assert!(v.spilled());
        v.clear();
        // Shrink to inline.
        v.grow(2);
        assert!(!v.spilled());
        assert_eq!(v.capacity(), 2);
        assert_eq!(v.len(), 0);
        v.push(4);
        assert_eq!(v[..], [4]);
    }

    #[test]
    fn resumable_extend() {
        let s = "a b c";
        // This iterator yields: (Some('a'), None, Some('b'), None, Some('c')), None
        let it = s
            .chars()
            .scan(0, |_, ch| if ch.is_whitespace() { None } else { Some(ch) });
        let mut v: SmallVec<[char; 4]> = SmallVec::new();
        v.extend(it);
        assert_eq!(v[..], ['a']);
    }

    // #139
    #[test]
    fn uninhabited() {
        enum Void {}
        let _sv = SmallVec::<[Void; 8]>::new();
    }

    #[test]
    fn grow_spilled_same_size() {
        let mut v: SmallVec<[u8; 2]> = SmallVec::new();
        v.push(0);
        v.push(1);
        v.push(2);
        assert!(v.spilled());
        assert_eq!(v.capacity(), 4);
        // grow with the same capacity
        v.grow(4);
        assert_eq!(v.capacity(), 4);
        assert_eq!(v[..], [0, 1, 2]);
    }

    #[cfg(feature = "const_generics")]
    #[test]
    fn const_generics() {
        let _v = SmallVec::<[i32; 987]>::default();
    }
}
