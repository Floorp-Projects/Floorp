/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![no_std]
#![deny(unsafe_code)]

//! A simple, fast, least-recently-used (LRU) cache.
//!
//! `LRUCache` uses a fixed-capacity array for storage. It provides `O(1)` insertion, and `O(n)`
//! lookup.  It does not require an allocator and can be used in `no_std` crates.
//!
//! See the [`LRUCache`](LRUCache) docs for details.

extern crate arrayvec;

use arrayvec::{Array, ArrayVec};

use core::fmt;

#[cfg(test)]
extern crate quickcheck;

#[cfg(test)]
#[macro_use(quickcheck)]
extern crate quickcheck_macros;

#[cfg(test)]
mod tests;

/// A LRU cache using a statically-sized array for storage.
///
/// `LRUCache` uses a fixed-capacity array for storage. It provides `O(1)` insertion, and `O(n)`
/// lookup.
///
/// All items are stored inline within the `LRUCache`, so it does not impose any heap allocation or
/// indirection.  A linked list is used to record the cache order, so the items themselves do not
/// need to be moved when the order changes.  (This is important for speed if the items are large.)
///
/// # Example
///
/// ```
/// use uluru::{LRUCache, Entry};
///
/// struct MyValue {
///     id: u32,
///     name: &'static str,
/// }
///
/// // A cache with a capacity of three.
/// type MyCache = LRUCache<[Entry<MyValue>; 3]>;
///
/// // Create an empty cache, then insert some items.
/// let mut cache = MyCache::default();
/// cache.insert(MyValue { id: 1, name: "Mercury" });
/// cache.insert(MyValue { id: 2, name: "Venus" });
/// cache.insert(MyValue { id: 3, name: "Earth" });
///
/// {
///     // Use the `find` method to retrieve an item from the cache.
///     // This also "touches" the item, marking it most-recently-used.
///     let item = cache.find(|x| x.id == 1);
///     assert_eq!(item.unwrap().name, "Mercury");
/// }
///
/// // If the cache is full, inserting a new item evicts the least-recently-used item:
/// cache.insert(MyValue { id: 4, name: "Mars" });
/// assert!(cache.find(|x| x.id == 2).is_none());
/// ```
pub struct LRUCache<A: Array> {
    /// The most-recently-used entry is at index `head`. The entries form a linked list, linked to
    /// each other by indices within the `entries` array.  After an entry is added to the array,
    /// its index never changes, so these links are never invalidated.
    entries: ArrayVec<A>,
    /// Index of the first entry. If the cache is empty, ignore this field.
    head: u16,
    /// Index of the last entry. If the cache is empty, ignore this field.
    tail: u16,
}

/// An entry in an LRUCache.
#[derive(Debug, Clone)]
pub struct Entry<T> {
    val: T,
    /// Index of the previous entry. If this entry is the head, ignore this field.
    prev: u16,
    /// Index of the next entry. If this entry is the tail, ignore this field.
    next: u16,
}

impl<A: Array> Default for LRUCache<A> {
    fn default() -> Self {
        let cache = LRUCache {
            entries: ArrayVec::new(),
            head: 0,
            tail: 0,
        };
        assert!(
            cache.entries.capacity() < u16::max_value() as usize,
            "Capacity overflow"
        );
        cache
    }
}

impl<T, A> LRUCache<A>
where
    A: Array<Item = Entry<T>>,
{
    /// Returns the number of elements in the cache.
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Returns the number of elements in the cache.
    #[inline]
    #[deprecated = "Use the 'len' method instead."]
    pub fn num_entries(&self) -> usize {
        self.len()
    }

    /// Returns the front entry in the list (most recently used).
    pub fn front(&self) -> Option<&T> {
        self.entries.get(self.head as usize).map(|e| &e.val)
    }

    /// Returns a mutable reference to the front entry in the list (most recently used).
    pub fn front_mut(&mut self) -> Option<&mut T> {
        self.entries.get_mut(self.head as usize).map(|e| &mut e.val)
    }

    /// Performs a lookup on the cache with the given test routine. Touches
    /// the result on a hit.
    pub fn lookup<F, R>(&mut self, mut test_one: F) -> Option<R>
    where
        F: FnMut(&mut T) -> Option<R>,
    {
        let mut result = None;
        let mut iter = self.iter_mut();
        while let Some((i, val)) = iter.next() {
            if let Some(r) = test_one(val) {
                result = Some((i, r));
                break;
            }
        }

        match result {
            None => None,
            Some((i, r)) => {
                self.touch_index(i);
                Some(r)
            }
        }
    }

    /// Touches the first item in the cache that matches the given predicate.
    /// Returns `true` on a hit, `false` if no matches.
    pub fn touch<F>(&mut self, mut pred: F) -> bool
    where
        F: FnMut(&T) -> bool,
    {
        let mut iter = self.iter_mut();
        while let Some((i, val)) = iter.next() {
            if pred(val) {
                self.touch_index(i);
                return true
            }
        }
        false
    }

    /// Returns the first item in the cache that matches the given predicate.
    /// Touches the result on a hit.
    pub fn find<F>(&mut self, pred: F) -> Option<&mut T>
    where
        F: FnMut(&T) -> bool,
    {
        if self.touch(pred) {
            self.front_mut()
        } else {
            None
        }
    }

    /// Insert a given key in the cache.
    ///
    /// This item becomes the front (most-recently-used) item in the cache.  If the cache is full,
    /// the back (least-recently-used) item will be removed.
    pub fn insert(&mut self, val: T) {
        let entry = Entry {
            val,
            prev: 0,
            next: 0,
        };

        // If the cache is full, replace the oldest entry. Otherwise, add an entry.
        let new_head = if self.entries.len() == self.entries.capacity() {
            let i = self.pop_back();
            self.entries[i as usize] = entry;
            i
        } else {
            self.entries.push(entry);
            self.entries.len() as u16 - 1
        };

        self.push_front(new_head);
    }

    /// Evict all elements from the cache.
    pub fn clear(&mut self) {
        self.entries.clear();
    }

    /// Evict all elements from the cache.
    #[inline]
    #[deprecated = "Use the 'clear' method instead."]
    pub fn evict_all(&mut self) {
        self.clear();
    }

    /// Iterate mutably over the contents of this cache.
    fn iter_mut(&mut self) -> IterMut<A> {
        IterMut {
            pos: self.head,
            cache: self,
        }
    }

    /// Touch a given entry, putting it first in the list.
    #[inline]
    fn touch_index(&mut self, idx: u16) {
        if idx != self.head {
            self.remove(idx);
            self.push_front(idx);
        }
    }

    /// Remove an entry from the linked list.
    ///
    /// Note: This only unlinks the entry from the list; it does not remove it from the array.
    fn remove(&mut self, i: u16) {
        let prev = self.entries[i as usize].prev;
        let next = self.entries[i as usize].next;

        if i == self.head {
            self.head = next;
        } else {
            self.entries[prev as usize].next = next;
        }

        if i == self.tail {
            self.tail = prev;
        } else {
            self.entries[next as usize].prev = prev;
        }
    }

    /// Insert a new entry at the head of the list.
    fn push_front(&mut self, i: u16) {
        if self.entries.len() == 1 {
            self.tail = i;
        } else {
            self.entries[i as usize].next = self.head;
            self.entries[self.head as usize].prev = i;
        }
        self.head = i;
    }

    /// Remove the last entry from the linked list. Returns the index of the removed entry.
    ///
    /// Note: This only unlinks the entry from the list; it does not remove it from the array.
    fn pop_back(&mut self) -> u16 {
        let old_tail = self.tail;
        let new_tail = self.entries[old_tail as usize].prev;
        self.tail = new_tail;
        old_tail
    }
}

impl<T, A> Clone for LRUCache<A>
where
    A: Array<Item = Entry<T>>,
    T: Clone,
{
    fn clone(&self) -> Self {
        Self {
            entries: self.entries.clone(),
            head: self.head,
            tail: self.tail,
        }
    }
}

impl<T, A> fmt::Debug for LRUCache<A>
where
    A: Array<Item = Entry<T>>,
    T: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("LRUCache")
            .field("head", &self.head)
            .field("tail", &self.tail)
            .field("entries", &self.entries)
            .finish()
    }
}

/// Mutable iterator over values in an LRUCache, from most-recently-used to least-recently-used.
struct IterMut<'a, A: 'a + Array> {
    cache: &'a mut LRUCache<A>,
    pos: u16,
}

impl<'a, A, T> IterMut<'a, A>
where
    A: Array<Item = Entry<T>>,
{
    fn next(&mut self) -> Option<(u16, &mut T)> {
        let index = self.pos;
        let entry = self.cache.entries.get_mut(index as usize)?;

        self.pos = if index == self.cache.tail {
            A::CAPACITY as u16 // Point past the end of the array to signal we are done.
        } else {
            entry.next
        };
        Some((index, &mut entry.val))
    }
}
