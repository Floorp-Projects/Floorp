#![warn(missing_docs, missing_debug_implementations)]
#![cfg_attr(test, warn(unreachable_pub))]
#![cfg_attr(not(feature = "std"), no_std)]

//! Pre-allocated storage for a uniform data type.
//!
//! `Slab` provides pre-allocated storage for a single data type. If many values
//! of a single type are being allocated, it can be more efficient to
//! pre-allocate the necessary storage. Since the size of the type is uniform,
//! memory fragmentation can be avoided. Storing, clearing, and lookup
//! operations become very cheap.
//!
//! While `Slab` may look like other Rust collections, it is not intended to be
//! used as a general purpose collection. The primary difference between `Slab`
//! and `Vec` is that `Slab` returns the key when storing the value.
//!
//! It is important to note that keys may be reused. In other words, once a
//! value associated with a given key is removed from a slab, that key may be
//! returned from future calls to `insert`.
//!
//! # Examples
//!
//! Basic storing and retrieval.
//!
//! ```
//! # use slab::*;
//! let mut slab = Slab::new();
//!
//! let hello = slab.insert("hello");
//! let world = slab.insert("world");
//!
//! assert_eq!(slab[hello], "hello");
//! assert_eq!(slab[world], "world");
//!
//! slab[world] = "earth";
//! assert_eq!(slab[world], "earth");
//! ```
//!
//! Sometimes it is useful to be able to associate the key with the value being
//! inserted in the slab. This can be done with the `vacant_entry` API as such:
//!
//! ```
//! # use slab::*;
//! let mut slab = Slab::new();
//!
//! let hello = {
//!     let entry = slab.vacant_entry();
//!     let key = entry.key();
//!
//!     entry.insert((key, "hello"));
//!     key
//! };
//!
//! assert_eq!(hello, slab[hello].0);
//! assert_eq!("hello", slab[hello].1);
//! ```
//!
//! It is generally a good idea to specify the desired capacity of a slab at
//! creation time. Note that `Slab` will grow the internal capacity when
//! attempting to insert a new value once the existing capacity has been reached.
//! To avoid this, add a check.
//!
//! ```
//! # use slab::*;
//! let mut slab = Slab::with_capacity(1024);
//!
//! // ... use the slab
//!
//! if slab.len() == slab.capacity() {
//!     panic!("slab full");
//! }
//!
//! slab.insert("the slab is not at capacity yet");
//! ```
//!
//! # Capacity and reallocation
//!
//! The capacity of a slab is the amount of space allocated for any future
//! values that will be inserted in the slab. This is not to be confused with
//! the *length* of the slab, which specifies the number of actual values
//! currently being inserted. If a slab's length is equal to its capacity, the
//! next value inserted into the slab will require growing the slab by
//! reallocating.
//!
//! For example, a slab with capacity 10 and length 0 would be an empty slab
//! with space for 10 more stored values. Storing 10 or fewer elements into the
//! slab will not change its capacity or cause reallocation to occur. However,
//! if the slab length is increased to 11 (due to another `insert`), it will
//! have to reallocate, which can be slow. For this reason, it is recommended to
//! use [`Slab::with_capacity`] whenever possible to specify how many values the
//! slab is expected to store.
//!
//! # Implementation
//!
//! `Slab` is backed by a `Vec` of slots. Each slot is either occupied or
//! vacant. `Slab` maintains a stack of vacant slots using a linked list. To
//! find a vacant slot, the stack is popped. When a slot is released, it is
//! pushed onto the stack.
//!
//! If there are no more available slots in the stack, then `Vec::reserve(1)` is
//! called and a new slot is created.
//!
//! [`Slab::with_capacity`]: struct.Slab.html#with_capacity

#[cfg(not(feature = "std"))]
extern crate alloc;
#[cfg(feature = "std")]
extern crate core;

#[cfg(feature = "serde")]
mod serde;

#[cfg(not(feature = "std"))]
use alloc::vec::Vec;

#[cfg(not(feature = "std"))]
use alloc::vec;

use core::iter::FromIterator;

use core::{fmt, mem, ops, slice};

#[cfg(feature = "std")]
use std::vec;

/// Pre-allocated storage for a uniform data type
///
/// See the [module documentation] for more details.
///
/// [module documentation]: index.html
#[derive(Clone)]
pub struct Slab<T> {
    // Chunk of memory
    entries: Vec<Entry<T>>,

    // Number of Filled elements currently in the slab
    len: usize,

    // Offset of the next available slot in the slab. Set to the slab's
    // capacity when the slab is full.
    next: usize,
}

impl<T> Default for Slab<T> {
    fn default() -> Self {
        Slab::new()
    }
}

/// A handle to a vacant entry in a `Slab`.
///
/// `VacantEntry` allows constructing values with the key that they will be
/// assigned to.
///
/// # Examples
///
/// ```
/// # use slab::*;
/// let mut slab = Slab::new();
///
/// let hello = {
///     let entry = slab.vacant_entry();
///     let key = entry.key();
///
///     entry.insert((key, "hello"));
///     key
/// };
///
/// assert_eq!(hello, slab[hello].0);
/// assert_eq!("hello", slab[hello].1);
/// ```
#[derive(Debug)]
pub struct VacantEntry<'a, T: 'a> {
    slab: &'a mut Slab<T>,
    key: usize,
}

/// A consuming iterator over the values stored in a `Slab`
pub struct IntoIter<T> {
    entries: vec::IntoIter<Entry<T>>,
    curr: usize,
}

/// An iterator over the values stored in the `Slab`
pub struct Iter<'a, T: 'a> {
    entries: slice::Iter<'a, Entry<T>>,
    curr: usize,
}

/// A mutable iterator over the values stored in the `Slab`
pub struct IterMut<'a, T: 'a> {
    entries: slice::IterMut<'a, Entry<T>>,
    curr: usize,
}

/// A draining iterator for `Slab`
pub struct Drain<'a, T: 'a>(vec::Drain<'a, Entry<T>>);

#[derive(Clone)]
enum Entry<T> {
    Vacant(usize),
    Occupied(T),
}

impl<T> Slab<T> {
    /// Construct a new, empty `Slab`.
    ///
    /// The function does not allocate and the returned slab will have no
    /// capacity until `insert` is called or capacity is explicitly reserved.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let slab: Slab<i32> = Slab::new();
    /// ```
    pub fn new() -> Slab<T> {
        Slab::with_capacity(0)
    }

    /// Construct a new, empty `Slab` with the specified capacity.
    ///
    /// The returned slab will be able to store exactly `capacity` without
    /// reallocating. If `capacity` is 0, the slab will not allocate.
    ///
    /// It is important to note that this function does not specify the *length*
    /// of the returned slab, but only the capacity. For an explanation of the
    /// difference between length and capacity, see [Capacity and
    /// reallocation](index.html#capacity-and-reallocation).
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::with_capacity(10);
    ///
    /// // The slab contains no values, even though it has capacity for more
    /// assert_eq!(slab.len(), 0);
    ///
    /// // These are all done without reallocating...
    /// for i in 0..10 {
    ///     slab.insert(i);
    /// }
    ///
    /// // ...but this may make the slab reallocate
    /// slab.insert(11);
    /// ```
    pub fn with_capacity(capacity: usize) -> Slab<T> {
        Slab {
            entries: Vec::with_capacity(capacity),
            next: 0,
            len: 0,
        }
    }

    /// Return the number of values the slab can store without reallocating.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let slab: Slab<i32> = Slab::with_capacity(10);
    /// assert_eq!(slab.capacity(), 10);
    /// ```
    pub fn capacity(&self) -> usize {
        self.entries.capacity()
    }

    /// Reserve capacity for at least `additional` more values to be stored
    /// without allocating.
    ///
    /// `reserve` does nothing if the slab already has sufficient capacity for
    /// `additional` more values. If more capacity is required, a new segment of
    /// memory will be allocated and all existing values will be copied into it.
    /// As such, if the slab is already very large, a call to `reserve` can end
    /// up being expensive.
    ///
    /// The slab may reserve more than `additional` extra space in order to
    /// avoid frequent reallocations. Use `reserve_exact` instead to guarantee
    /// that only the requested space is allocated.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// slab.insert("hello");
    /// slab.reserve(10);
    /// assert!(slab.capacity() >= 11);
    /// ```
    pub fn reserve(&mut self, additional: usize) {
        if self.capacity() - self.len >= additional {
            return;
        }
        let need_add = additional - (self.entries.len() - self.len);
        self.entries.reserve(need_add);
    }

    /// Reserve the minimum capacity required to store exactly `additional`
    /// more values.
    ///
    /// `reserve_exact` does nothing if the slab already has sufficient capacity
    /// for `additional` more valus. If more capacity is required, a new segment
    /// of memory will be allocated and all existing values will be copied into
    /// it.  As such, if the slab is already very large, a call to `reserve` can
    /// end up being expensive.
    ///
    /// Note that the allocator may give the slab more space than it requests.
    /// Therefore capacity can not be relied upon to be precisely minimal.
    /// Prefer `reserve` if future insertions are expected.
    ///
    /// # Panics
    ///
    /// Panics if the new capacity overflows `usize`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// slab.insert("hello");
    /// slab.reserve_exact(10);
    /// assert!(slab.capacity() >= 11);
    /// ```
    pub fn reserve_exact(&mut self, additional: usize) {
        if self.capacity() - self.len >= additional {
            return;
        }
        let need_add = additional - (self.entries.len() - self.len);
        self.entries.reserve_exact(need_add);
    }

    /// Shrink the capacity of the slab as much as possible without invalidating keys.
    ///
    /// Because values cannot be moved to a different index, the slab cannot
    /// shrink past any stored values.
    /// It will drop down as close as possible to the length but the allocator may
    /// still inform the underlying vector that there is space for a few more elements.
    ///
    /// This function can take O(n) time even when the capacity cannot be reduced
    /// or the allocation is shrunk in place. Repeated calls run in O(1) though.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::with_capacity(10);
    ///
    /// for i in 0..3 {
    ///     slab.insert(i);
    /// }
    ///
    /// slab.shrink_to_fit();
    /// assert!(slab.capacity() >= 3 && slab.capacity() < 10);
    /// ```
    ///
    /// The slab cannot shrink past the last present value even if previous
    /// values are removed:
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::with_capacity(10);
    ///
    /// for i in 0..4 {
    ///     slab.insert(i);
    /// }
    ///
    /// slab.remove(0);
    /// slab.remove(3);
    ///
    /// slab.shrink_to_fit();
    /// assert!(slab.capacity() >= 3 && slab.capacity() < 10);
    /// ```
    pub fn shrink_to_fit(&mut self) {
        // Remove all vacant entries after the last occupied one, so that
        // the capacity can be reduced to what is actually needed.
        // If the slab is empty the vector can simply be cleared, but that
        // optimization would not affect time complexity when T: Drop.
        let len_before = self.entries.len();
        while let Some(&Entry::Vacant(_)) = self.entries.last() {
            self.entries.pop();
        }

        // Removing entries breaks the list of vacant entries,
        // so it must be repaired
        if self.entries.len() != len_before {
            // Some vacant entries were removed, so the list now likely¹
            // either contains references to the removed entries, or has an
            // invalid end marker. Fix this by recreating the list.
            self.recreate_vacant_list();
            // ¹: If the removed entries formed the tail of the list, with the
            // most recently popped entry being the head of them, (so that its
            // index is now the end marker) the list is still valid.
            // Checking for that unlikely scenario of this infrequently called
            // is not worth the code complexity.
        }

        self.entries.shrink_to_fit();
    }

    /// Iterate through all entries to recreate and repair the vacant list.
    /// self.len must be correct and is not modified.
    fn recreate_vacant_list(&mut self) {
        self.next = self.entries.len();
        // We can stop once we've found all vacant entries
        let mut remaining_vacant = self.entries.len() - self.len;
        // Iterate in reverse order so that lower keys are at the start of
        // the vacant list. This way future shrinks are more likely to be
        // able to remove vacant entries.
        for (i, entry) in self.entries.iter_mut().enumerate().rev() {
            if remaining_vacant == 0 {
                break;
            }
            if let Entry::Vacant(ref mut next) = *entry {
                *next = self.next;
                self.next = i;
                remaining_vacant -= 1;
            }
        }
    }

    /// Reduce the capacity as much as possible, changing the key for elements when necessary.
    ///
    /// To allow updating references to the elements which must be moved to a new key,
    /// this function takes a closure which is called before moving each element.
    /// The second and third parameters to the closure are the current key and
    /// new key respectively.
    /// In case changing the key for one element turns out not to be possible,
    /// the move can be cancelled by returning `false` from the closure.
    /// In that case no further attempts at relocating elements is made.
    /// If the closure unwinds, the slab will be left in a consistent state,
    /// but the value that the closure panicked on might be removed.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    ///
    /// let mut slab = Slab::with_capacity(10);
    /// let a = slab.insert('a');
    /// slab.insert('b');
    /// slab.insert('c');
    /// slab.remove(a);
    /// slab.compact(|&mut value, from, to| {
    ///     assert_eq!((value, from, to), ('c', 2, 0));
    ///     true
    /// });
    /// assert!(slab.capacity() >= 2 && slab.capacity() < 10);
    /// ```
    ///
    /// The value is not moved when the closure returns `Err`:
    ///
    /// ```
    /// # use slab::*;
    ///
    /// let mut slab = Slab::with_capacity(100);
    /// let a = slab.insert('a');
    /// let b = slab.insert('b');
    /// slab.remove(a);
    /// slab.compact(|&mut value, from, to| false);
    /// assert_eq!(slab.iter().next(), Some((b, &'b')));
    /// ```
    pub fn compact<F>(&mut self, mut rekey: F)
    where
        F: FnMut(&mut T, usize, usize) -> bool,
    {
        // If the closure unwinds, we need to restore a valid list of vacant entries
        struct CleanupGuard<'a, T: 'a> {
            slab: &'a mut Slab<T>,
            decrement: bool,
        }
        impl<'a, T: 'a> Drop for CleanupGuard<'a, T> {
            fn drop(&mut self) {
                if self.decrement {
                    // Value was popped and not pushed back on
                    self.slab.len -= 1;
                }
                self.slab.recreate_vacant_list();
            }
        }
        let mut guard = CleanupGuard {
            slab: self,
            decrement: true,
        };

        let mut occupied_until = 0;
        // While there are vacant entries
        while guard.slab.entries.len() > guard.slab.len {
            // Find a value that needs to be moved,
            // by popping entries until we find an occopied one.
            // (entries cannot be empty because 0 is not greater than anything)
            if let Some(Entry::Occupied(mut value)) = guard.slab.entries.pop() {
                // Found one, now find a vacant entry to move it to
                while let Some(&Entry::Occupied(_)) = guard.slab.entries.get(occupied_until) {
                    occupied_until += 1;
                }
                // Let the caller try to update references to the key
                if !rekey(&mut value, guard.slab.entries.len(), occupied_until) {
                    // Changing the key failed, so push the entry back on at its old index.
                    guard.slab.entries.push(Entry::Occupied(value));
                    guard.decrement = false;
                    guard.slab.entries.shrink_to_fit();
                    return;
                    // Guard drop handles cleanup
                }
                // Put the value in its new spot
                guard.slab.entries[occupied_until] = Entry::Occupied(value);
                // ... and mark it as occupied (this is optional)
                occupied_until += 1;
            }
        }
        guard.slab.next = guard.slab.len;
        guard.slab.entries.shrink_to_fit();
        // Normal cleanup is not necessary
        mem::forget(guard);
    }

    /// Clear the slab of all values.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// for i in 0..3 {
    ///     slab.insert(i);
    /// }
    ///
    /// slab.clear();
    /// assert!(slab.is_empty());
    /// ```
    pub fn clear(&mut self) {
        self.entries.clear();
        self.len = 0;
        self.next = 0;
    }

    /// Return the number of stored values.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// for i in 0..3 {
    ///     slab.insert(i);
    /// }
    ///
    /// assert_eq!(3, slab.len());
    /// ```
    pub fn len(&self) -> usize {
        self.len
    }

    /// Return `true` if there are no values stored in the slab.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// assert!(slab.is_empty());
    ///
    /// slab.insert(1);
    /// assert!(!slab.is_empty());
    /// ```
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Return an iterator over the slab.
    ///
    /// This function should generally be **avoided** as it is not efficient.
    /// Iterators must iterate over every slot in the slab even if it is
    /// vacant. As such, a slab with a capacity of 1 million but only one
    /// stored value must still iterate the million slots.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// for i in 0..3 {
    ///     slab.insert(i);
    /// }
    ///
    /// let mut iterator = slab.iter();
    ///
    /// assert_eq!(iterator.next(), Some((0, &0)));
    /// assert_eq!(iterator.next(), Some((1, &1)));
    /// assert_eq!(iterator.next(), Some((2, &2)));
    /// assert_eq!(iterator.next(), None);
    /// ```
    pub fn iter(&self) -> Iter<T> {
        Iter {
            entries: self.entries.iter(),
            curr: 0,
        }
    }

    /// Return an iterator that allows modifying each value.
    ///
    /// This function should generally be **avoided** as it is not efficient.
    /// Iterators must iterate over every slot in the slab even if it is
    /// vacant. As such, a slab with a capacity of 1 million but only one
    /// stored value must still iterate the million slots.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let key1 = slab.insert(0);
    /// let key2 = slab.insert(1);
    ///
    /// for (key, val) in slab.iter_mut() {
    ///     if key == key1 {
    ///         *val += 2;
    ///     }
    /// }
    ///
    /// assert_eq!(slab[key1], 2);
    /// assert_eq!(slab[key2], 1);
    /// ```
    pub fn iter_mut(&mut self) -> IterMut<T> {
        IterMut {
            entries: self.entries.iter_mut(),
            curr: 0,
        }
    }

    /// Return a reference to the value associated with the given key.
    ///
    /// If the given key is not associated with a value, then `None` is
    /// returned.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// let key = slab.insert("hello");
    ///
    /// assert_eq!(slab.get(key), Some(&"hello"));
    /// assert_eq!(slab.get(123), None);
    /// ```
    pub fn get(&self, key: usize) -> Option<&T> {
        match self.entries.get(key) {
            Some(&Entry::Occupied(ref val)) => Some(val),
            _ => None,
        }
    }

    /// Return a mutable reference to the value associated with the given key.
    ///
    /// If the given key is not associated with a value, then `None` is
    /// returned.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// let key = slab.insert("hello");
    ///
    /// *slab.get_mut(key).unwrap() = "world";
    ///
    /// assert_eq!(slab[key], "world");
    /// assert_eq!(slab.get_mut(123), None);
    /// ```
    pub fn get_mut(&mut self, key: usize) -> Option<&mut T> {
        match self.entries.get_mut(key) {
            Some(&mut Entry::Occupied(ref mut val)) => Some(val),
            _ => None,
        }
    }

    /// Return two mutable references to the values associated with the two
    /// given keys simultaneously.
    ///
    /// If any one of the given keys is not associated with a value, then `None`
    /// is returned.
    ///
    /// This function can be used to get two mutable references out of one slab,
    /// so that you can manipulate both of them at the same time, eg. swap them.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// use std::mem;
    ///
    /// let mut slab = Slab::new();
    /// let key1 = slab.insert(1);
    /// let key2 = slab.insert(2);
    /// let (value1, value2) = slab.get2_mut(key1, key2).unwrap();
    /// mem::swap(value1, value2);
    /// assert_eq!(slab[key1], 2);
    /// assert_eq!(slab[key2], 1);
    /// ```
    pub fn get2_mut(&mut self, key1: usize, key2: usize) -> Option<(&mut T, &mut T)> {
        assert!(key1 != key2);

        let (entry1, entry2);

        if key1 > key2 {
            let (slice1, slice2) = self.entries.split_at_mut(key1);
            entry1 = slice2.get_mut(0);
            entry2 = slice1.get_mut(key2);
        } else {
            let (slice1, slice2) = self.entries.split_at_mut(key2);
            entry1 = slice1.get_mut(key1);
            entry2 = slice2.get_mut(0);
        }

        match (entry1, entry2) {
            (
                Some(&mut Entry::Occupied(ref mut val1)),
                Some(&mut Entry::Occupied(ref mut val2)),
            ) => Some((val1, val2)),
            _ => None,
        }
    }

    /// Return a reference to the value associated with the given key without
    /// performing bounds checking.
    ///
    /// This function should be used with care.
    ///
    /// # Safety
    ///
    /// The key must be within bounds.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// let key = slab.insert(2);
    ///
    /// unsafe {
    ///     assert_eq!(slab.get_unchecked(key), &2);
    /// }
    /// ```
    pub unsafe fn get_unchecked(&self, key: usize) -> &T {
        match *self.entries.get_unchecked(key) {
            Entry::Occupied(ref val) => val,
            _ => unreachable!(),
        }
    }

    /// Return a mutable reference to the value associated with the given key
    /// without performing bounds checking.
    ///
    /// This function should be used with care.
    ///
    /// # Safety
    ///
    /// The key must be within bounds.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// let key = slab.insert(2);
    ///
    /// unsafe {
    ///     let val = slab.get_unchecked_mut(key);
    ///     *val = 13;
    /// }
    ///
    /// assert_eq!(slab[key], 13);
    /// ```
    pub unsafe fn get_unchecked_mut(&mut self, key: usize) -> &mut T {
        match *self.entries.get_unchecked_mut(key) {
            Entry::Occupied(ref mut val) => val,
            _ => unreachable!(),
        }
    }

    /// Return two mutable references to the values associated with the two
    /// given keys simultaneously without performing bounds checking and safety
    /// condition checking.
    ///
    /// This function should be used with care.
    ///
    /// # Safety
    ///
    /// - Both keys must be within bounds.
    /// - The condition `key1 != key2` must hold.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// use std::mem;
    ///
    /// let mut slab = Slab::new();
    /// let key1 = slab.insert(1);
    /// let key2 = slab.insert(2);
    /// let (value1, value2) = unsafe { slab.get2_unchecked_mut(key1, key2) };
    /// mem::swap(value1, value2);
    /// assert_eq!(slab[key1], 2);
    /// assert_eq!(slab[key2], 1);
    /// ```
    pub unsafe fn get2_unchecked_mut(&mut self, key1: usize, key2: usize) -> (&mut T, &mut T) {
        let ptr1 = self.entries.get_unchecked_mut(key1) as *mut Entry<T>;
        let ptr2 = self.entries.get_unchecked_mut(key2) as *mut Entry<T>;
        match (&mut *ptr1, &mut *ptr2) {
            (&mut Entry::Occupied(ref mut val1), &mut Entry::Occupied(ref mut val2)) => {
                (val1, val2)
            }
            _ => unreachable!(),
        }
    }

    /// Get the key for an element in the slab.
    ///
    /// The reference must point to an element owned by the slab.
    /// Otherwise this function will panic.
    /// This is a constant-time operation because the key can be calculated
    /// from the reference with pointer arithmetic.
    ///
    /// # Panics
    ///
    /// This function will panic if the reference does not point to an element
    /// of the slab.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    ///
    /// let mut slab = Slab::new();
    /// let key = slab.insert(String::from("foo"));
    /// let value = &slab[key];
    /// assert_eq!(slab.key_of(value), key);
    /// ```
    ///
    /// Values are not compared, so passing a reference to a different locaton
    /// will result in a panic:
    ///
    /// ```should_panic
    /// # use slab::*;
    ///
    /// let mut slab = Slab::new();
    /// let key = slab.insert(0);
    /// let bad = &0;
    /// slab.key_of(bad); // this will panic
    /// unreachable!();
    /// ```
    pub fn key_of(&self, present_element: &T) -> usize {
        let element_ptr = present_element as *const T as usize;
        let base_ptr = self.entries.as_ptr() as usize;
        // Use wrapping subtraction in case the reference is bad
        let byte_offset = element_ptr.wrapping_sub(base_ptr);
        // The division rounds away any offset of T inside Entry
        // The size of Entry<T> is never zero even if T is due to Vacant(usize)
        let key = byte_offset / mem::size_of::<Entry<T>>();
        // Prevent returning unspecified (but out of bounds) values
        if key >= self.entries.len() {
            panic!("The reference points to a value outside this slab");
        }
        // The reference cannot point to a vacant entry, because then it would not be valid
        key
    }

    /// Insert a value in the slab, returning key assigned to the value.
    ///
    /// The returned key can later be used to retrieve or remove the value using indexed
    /// lookup and `remove`. Additional capacity is allocated if needed. See
    /// [Capacity and reallocation](index.html#capacity-and-reallocation).
    ///
    /// # Panics
    ///
    /// Panics if the number of elements in the vector overflows a `usize`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    /// let key = slab.insert("hello");
    /// assert_eq!(slab[key], "hello");
    /// ```
    pub fn insert(&mut self, val: T) -> usize {
        let key = self.next;

        self.insert_at(key, val);

        key
    }

    /// Return a handle to a vacant entry allowing for further manipulation.
    ///
    /// This function is useful when creating values that must contain their
    /// slab key. The returned `VacantEntry` reserves a slot in the slab and is
    /// able to query the associated key.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let hello = {
    ///     let entry = slab.vacant_entry();
    ///     let key = entry.key();
    ///
    ///     entry.insert((key, "hello"));
    ///     key
    /// };
    ///
    /// assert_eq!(hello, slab[hello].0);
    /// assert_eq!("hello", slab[hello].1);
    /// ```
    pub fn vacant_entry(&mut self) -> VacantEntry<T> {
        VacantEntry {
            key: self.next,
            slab: self,
        }
    }

    fn insert_at(&mut self, key: usize, val: T) {
        self.len += 1;

        if key == self.entries.len() {
            self.entries.push(Entry::Occupied(val));
            self.next = key + 1;
        } else {
            self.next = match self.entries.get(key) {
                Some(&Entry::Vacant(next)) => next,
                _ => unreachable!(),
            };
            self.entries[key] = Entry::Occupied(val);
        }
    }

    /// Remove and return the value associated with the given key.
    ///
    /// The key is then released and may be associated with future stored
    /// values.
    ///
    /// # Panics
    ///
    /// Panics if `key` is not associated with a value.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let hello = slab.insert("hello");
    ///
    /// assert_eq!(slab.remove(hello), "hello");
    /// assert!(!slab.contains(hello));
    /// ```
    pub fn remove(&mut self, key: usize) -> T {
        if let Some(entry) = self.entries.get_mut(key) {
            // Swap the entry at the provided value
            let prev = mem::replace(entry, Entry::Vacant(self.next));

            match prev {
                Entry::Occupied(val) => {
                    self.len -= 1;
                    self.next = key;
                    return val;
                }
                _ => {
                    // Woops, the entry is actually vacant, restore the state
                    *entry = prev;
                }
            }
        }
        panic!("invalid key");
    }

    /// Return `true` if a value is associated with the given key.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let hello = slab.insert("hello");
    /// assert!(slab.contains(hello));
    ///
    /// slab.remove(hello);
    ///
    /// assert!(!slab.contains(hello));
    /// ```
    pub fn contains(&self, key: usize) -> bool {
        match self.entries.get(key) {
            Some(&Entry::Occupied(_)) => true,
            _ => false,
        }
    }

    /// Retain only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(usize, &mut e)`
    /// returns false. This method operates in place and preserves the key
    /// associated with the retained values.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let k1 = slab.insert(0);
    /// let k2 = slab.insert(1);
    /// let k3 = slab.insert(2);
    ///
    /// slab.retain(|key, val| key == k1 || *val == 1);
    ///
    /// assert!(slab.contains(k1));
    /// assert!(slab.contains(k2));
    /// assert!(!slab.contains(k3));
    ///
    /// assert_eq!(2, slab.len());
    /// ```
    pub fn retain<F>(&mut self, mut f: F)
    where
        F: FnMut(usize, &mut T) -> bool,
    {
        for i in 0..self.entries.len() {
            let keep = match self.entries[i] {
                Entry::Occupied(ref mut v) => f(i, v),
                _ => true,
            };

            if !keep {
                self.remove(i);
            }
        }
    }

    /// Return a draining iterator that removes all elements from the slab and
    /// yields the removed items.
    ///
    /// Note: Elements are removed even if the iterator is only partially
    /// consumed or not consumed at all.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let _ = slab.insert(0);
    /// let _ = slab.insert(1);
    /// let _ = slab.insert(2);
    ///
    /// {
    ///     let mut drain = slab.drain();
    ///
    ///     assert_eq!(Some(0), drain.next());
    ///     assert_eq!(Some(1), drain.next());
    ///     assert_eq!(Some(2), drain.next());
    ///     assert_eq!(None, drain.next());
    /// }
    ///
    /// assert!(slab.is_empty());
    /// ```
    pub fn drain(&mut self) -> Drain<T> {
        self.len = 0;
        self.next = 0;
        Drain(self.entries.drain(..))
    }
}

impl<T> ops::Index<usize> for Slab<T> {
    type Output = T;

    fn index(&self, key: usize) -> &T {
        match self.entries.get(key) {
            Some(&Entry::Occupied(ref v)) => v,
            _ => panic!("invalid key"),
        }
    }
}

impl<T> ops::IndexMut<usize> for Slab<T> {
    fn index_mut(&mut self, key: usize) -> &mut T {
        match self.entries.get_mut(key) {
            Some(&mut Entry::Occupied(ref mut v)) => v,
            _ => panic!("invalid key"),
        }
    }
}

impl<T> IntoIterator for Slab<T> {
    type Item = (usize, T);
    type IntoIter = IntoIter<T>;

    fn into_iter(self) -> IntoIter<T> {
        IntoIter {
            entries: self.entries.into_iter(),
            curr: 0,
        }
    }
}

impl<'a, T> IntoIterator for &'a Slab<T> {
    type Item = (usize, &'a T);
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut Slab<T> {
    type Item = (usize, &'a mut T);
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> IterMut<'a, T> {
        self.iter_mut()
    }
}

/// Create a slab from an iterator of key-value pairs.
///
/// If the iterator produces duplicate keys, the previous value is replaced with the later one.
/// The keys does not need to be sorted beforehand, and this function always
/// takes O(n) time.
/// Note that the returned slab will use space proportional to the largest key,
/// so don't use `Slab` with untrusted keys.
///
/// # Examples
///
/// ```
/// # use slab::*;
///
/// let vec = vec![(2,'a'), (6,'b'), (7,'c')];
/// let slab = vec.into_iter().collect::<Slab<char>>();
/// assert_eq!(slab.len(), 3);
/// assert!(slab.capacity() >= 8);
/// assert_eq!(slab[2], 'a');
/// ```
///
/// With duplicate and unsorted keys:
///
/// ```
/// # use slab::*;
///
/// let vec = vec![(20,'a'), (10,'b'), (11,'c'), (10,'d')];
/// let slab = vec.into_iter().collect::<Slab<char>>();
/// assert_eq!(slab.len(), 3);
/// assert_eq!(slab[10], 'd');
/// ```
impl<T> FromIterator<(usize, T)> for Slab<T> {
    fn from_iter<I>(iterable: I) -> Self
    where
        I: IntoIterator<Item = (usize, T)>,
    {
        let iterator = iterable.into_iter();
        let mut slab = Self::with_capacity(iterator.size_hint().0);

        let mut vacant_list_broken = false;
        for (key, value) in iterator {
            if key < slab.entries.len() {
                // iterator is not sorted, might need to recreate vacant list
                if let Entry::Vacant(_) = slab.entries[key] {
                    vacant_list_broken = true;
                    slab.len += 1;
                }
                // if an element with this key already exists, replace it.
                // This is consisent with HashMap and BtreeMap
                slab.entries[key] = Entry::Occupied(value);
            } else {
                // insert holes as necessary
                while slab.entries.len() < key {
                    // add the entry to the start of the vacant list
                    let next = slab.next;
                    slab.next = slab.entries.len();
                    slab.entries.push(Entry::Vacant(next));
                }
                slab.entries.push(Entry::Occupied(value));
                slab.len += 1;
            }
        }
        if slab.len == slab.entries.len() {
            // no vacant enries, so next might not have been updated
            slab.next = slab.entries.len();
        } else if vacant_list_broken {
            slab.recreate_vacant_list();
        }
        slab
    }
}

impl<T> fmt::Debug for Slab<T>
where
    T: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Slab")
            .field("len", &self.len)
            .field("cap", &self.capacity())
            .finish()
    }
}

impl<T> fmt::Debug for IntoIter<T>
where
    T: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Iter")
            .field("curr", &self.curr)
            .field("remaining", &self.entries.len())
            .finish()
    }
}

impl<'a, T: 'a> fmt::Debug for Iter<'a, T>
where
    T: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Iter")
            .field("curr", &self.curr)
            .field("remaining", &self.entries.len())
            .finish()
    }
}

impl<'a, T: 'a> fmt::Debug for IterMut<'a, T>
where
    T: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("IterMut")
            .field("curr", &self.curr)
            .field("remaining", &self.entries.len())
            .finish()
    }
}

impl<'a, T: 'a> fmt::Debug for Drain<'a, T> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Drain").finish()
    }
}

// ===== VacantEntry =====

impl<'a, T> VacantEntry<'a, T> {
    /// Insert a value in the entry, returning a mutable reference to the value.
    ///
    /// To get the key associated with the value, use `key` prior to calling
    /// `insert`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let hello = {
    ///     let entry = slab.vacant_entry();
    ///     let key = entry.key();
    ///
    ///     entry.insert((key, "hello"));
    ///     key
    /// };
    ///
    /// assert_eq!(hello, slab[hello].0);
    /// assert_eq!("hello", slab[hello].1);
    /// ```
    pub fn insert(self, val: T) -> &'a mut T {
        self.slab.insert_at(self.key, val);

        match self.slab.entries.get_mut(self.key) {
            Some(&mut Entry::Occupied(ref mut v)) => v,
            _ => unreachable!(),
        }
    }

    /// Return the key associated with this entry.
    ///
    /// A value stored in this entry will be associated with this key.
    ///
    /// # Examples
    ///
    /// ```
    /// # use slab::*;
    /// let mut slab = Slab::new();
    ///
    /// let hello = {
    ///     let entry = slab.vacant_entry();
    ///     let key = entry.key();
    ///
    ///     entry.insert((key, "hello"));
    ///     key
    /// };
    ///
    /// assert_eq!(hello, slab[hello].0);
    /// assert_eq!("hello", slab[hello].1);
    /// ```
    pub fn key(&self) -> usize {
        self.key
    }
}

// ===== IntoIter =====

impl<T> Iterator for IntoIter<T> {
    type Item = (usize, T);

    fn next(&mut self) -> Option<(usize, T)> {
        while let Some(entry) = self.entries.next() {
            let curr = self.curr;
            self.curr += 1;

            if let Entry::Occupied(v) = entry {
                return Some((curr, v));
            }
        }

        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some(self.entries.len()))
    }
}

impl<T> DoubleEndedIterator for IntoIter<T> {
    fn next_back(&mut self) -> Option<(usize, T)> {
        while let Some(entry) = self.entries.next_back() {
            if let Entry::Occupied(v) = entry {
                let key = self.curr + self.entries.len();
                return Some((key, v));
            }
        }

        None
    }
}

// ===== Iter =====

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = (usize, &'a T);

    fn next(&mut self) -> Option<(usize, &'a T)> {
        while let Some(entry) = self.entries.next() {
            let curr = self.curr;
            self.curr += 1;

            if let Entry::Occupied(ref v) = *entry {
                return Some((curr, v));
            }
        }

        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some(self.entries.len()))
    }
}

impl<'a, T> DoubleEndedIterator for Iter<'a, T> {
    fn next_back(&mut self) -> Option<(usize, &'a T)> {
        while let Some(entry) = self.entries.next_back() {
            if let Entry::Occupied(ref v) = *entry {
                let key = self.curr + self.entries.len();
                return Some((key, v));
            }
        }

        None
    }
}

// ===== IterMut =====

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = (usize, &'a mut T);

    fn next(&mut self) -> Option<(usize, &'a mut T)> {
        while let Some(entry) = self.entries.next() {
            let curr = self.curr;
            self.curr += 1;

            if let Entry::Occupied(ref mut v) = *entry {
                return Some((curr, v));
            }
        }

        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some(self.entries.len()))
    }
}

impl<'a, T> DoubleEndedIterator for IterMut<'a, T> {
    fn next_back(&mut self) -> Option<(usize, &'a mut T)> {
        while let Some(entry) = self.entries.next_back() {
            if let Entry::Occupied(ref mut v) = *entry {
                let key = self.curr + self.entries.len();
                return Some((key, v));
            }
        }

        None
    }
}

// ===== Drain =====

impl<'a, T> Iterator for Drain<'a, T> {
    type Item = T;

    fn next(&mut self) -> Option<T> {
        while let Some(entry) = self.0.next() {
            if let Entry::Occupied(v) = entry {
                return Some(v);
            }
        }

        None
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (0, Some(self.0.len()))
    }
}

impl<'a, T> DoubleEndedIterator for Drain<'a, T> {
    fn next_back(&mut self) -> Option<T> {
        while let Some(entry) = self.0.next_back() {
            if let Entry::Occupied(v) = entry {
                return Some(v);
            }
        }

        None
    }
}
