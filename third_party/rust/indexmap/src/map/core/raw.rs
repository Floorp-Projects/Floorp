#![allow(unsafe_code)]
//! This module encapsulates the `unsafe` access to `hashbrown::raw::RawTable`,
//! mostly in dealing with its bucket "pointers".

use super::{Entry, Equivalent, HashValue, IndexMapCore, VacantEntry};
use crate::util::enumerate;
use core::fmt;
use core::mem::replace;
use hashbrown::raw::RawTable;

type RawBucket = hashbrown::raw::Bucket<usize>;

pub(super) struct DebugIndices<'a>(pub &'a RawTable<usize>);
impl fmt::Debug for DebugIndices<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let indices = unsafe { self.0.iter().map(|raw_bucket| raw_bucket.read()) };
        f.debug_list().entries(indices).finish()
    }
}

impl<K, V> IndexMapCore<K, V> {
    /// Return the raw bucket with an equivalent key
    fn find_equivalent<Q>(&self, hash: HashValue, key: &Q) -> Option<RawBucket>
    where
        Q: ?Sized + Equivalent<K>,
    {
        self.indices.find(hash.get(), {
            move |&i| Q::equivalent(key, &self.entries[i].key)
        })
    }

    /// Return the raw bucket for the given index
    fn find_index(&self, hash: HashValue, index: usize) -> Option<RawBucket> {
        self.indices.find(hash.get(), move |&i| i == index)
    }

    /// Return the index in `entries` where an equivalent key can be found
    pub(crate) fn get_index_of<Q>(&self, hash: HashValue, key: &Q) -> Option<usize>
    where
        Q: ?Sized + Equivalent<K>,
    {
        match self.find_equivalent(hash, key) {
            Some(raw_bucket) => Some(unsafe { raw_bucket.read() }),
            None => None,
        }
    }

    /// Erase the given index from `indices`.
    ///
    /// The index doesn't need to be valid in `entries` while calling this.  No other index
    /// adjustments are made -- this is only used by `pop` for the greatest index.
    pub(super) fn erase_index(&mut self, hash: HashValue, index: usize) {
        debug_assert_eq!(index, self.indices.len() - 1);
        let raw_bucket = self.find_index(hash, index).unwrap();
        unsafe { self.indices.erase(raw_bucket) };
    }

    /// Erase `start..end` from `indices`, and shift `end..` indices down to `start..`
    ///
    /// All of these items should still be at their original location in `entries`.
    /// This is used by `drain`, which will let `Vec::drain` do the work on `entries`.
    pub(super) fn erase_indices(&mut self, start: usize, end: usize) {
        let (init, shifted_entries) = self.entries.split_at(end);
        let (start_entries, erased_entries) = init.split_at(start);

        let erased = erased_entries.len();
        let shifted = shifted_entries.len();
        let half_capacity = self.indices.buckets() / 2;

        // Use a heuristic between different strategies
        if erased == 0 {
            // Degenerate case, nothing to do
        } else if start + shifted < half_capacity && start < erased {
            // Reinsert everything, as there are few kept indices
            self.indices.clear();

            // Reinsert stable indices
            for (i, entry) in enumerate(start_entries) {
                self.indices.insert_no_grow(entry.hash.get(), i);
            }

            // Reinsert shifted indices
            for (i, entry) in (start..).zip(shifted_entries) {
                self.indices.insert_no_grow(entry.hash.get(), i);
            }
        } else if erased + shifted < half_capacity {
            // Find each affected index, as there are few to adjust

            // Find erased indices
            for (i, entry) in (start..).zip(erased_entries) {
                let bucket = self.find_index(entry.hash, i).unwrap();
                unsafe { self.indices.erase(bucket) };
            }

            // Find shifted indices
            for ((new, old), entry) in (start..).zip(end..).zip(shifted_entries) {
                let bucket = self.find_index(entry.hash, old).unwrap();
                unsafe { bucket.write(new) };
            }
        } else {
            // Sweep the whole table for adjustments
            unsafe {
                for bucket in self.indices.iter() {
                    let i = bucket.read();
                    if i >= end {
                        bucket.write(i - erased);
                    } else if i >= start {
                        self.indices.erase(bucket);
                    }
                }
            }
        }

        debug_assert_eq!(self.indices.len(), start + shifted);
    }

    pub(crate) fn entry(&mut self, hash: HashValue, key: K) -> Entry<'_, K, V>
    where
        K: Eq,
    {
        match self.find_equivalent(hash, &key) {
            // Safety: The entry is created with a live raw bucket, at the same time we have a &mut
            // reference to the map, so it can not be modified further.
            Some(raw_bucket) => Entry::Occupied(OccupiedEntry {
                map: self,
                raw_bucket,
                key,
            }),
            None => Entry::Vacant(VacantEntry {
                map: self,
                hash,
                key,
            }),
        }
    }

    /// Remove an entry by shifting all entries that follow it
    pub(crate) fn shift_remove_full<Q>(&mut self, hash: HashValue, key: &Q) -> Option<(usize, K, V)>
    where
        Q: ?Sized + Equivalent<K>,
    {
        match self.find_equivalent(hash, key) {
            Some(raw_bucket) => unsafe { Some(self.shift_remove_bucket(raw_bucket)) },
            None => None,
        }
    }

    /// Remove an entry by shifting all entries that follow it
    pub(crate) fn shift_remove_index(&mut self, index: usize) -> Option<(K, V)> {
        let raw_bucket = match self.entries.get(index) {
            Some(entry) => self.find_index(entry.hash, index).unwrap(),
            None => return None,
        };
        unsafe {
            let (_, key, value) = self.shift_remove_bucket(raw_bucket);
            Some((key, value))
        }
    }

    /// Remove an entry by shifting all entries that follow it
    ///
    /// Safety: The caller must pass a live `raw_bucket`.
    #[allow(unused_unsafe)]
    unsafe fn shift_remove_bucket(&mut self, raw_bucket: RawBucket) -> (usize, K, V) {
        // use Vec::remove, but then we need to update the indices that point
        // to all of the other entries that have to move
        let index = unsafe { self.indices.remove(raw_bucket) };
        let entry = self.entries.remove(index);

        // correct indices that point to the entries that followed the removed entry.
        // use a heuristic between a full sweep vs. a `find()` for every shifted item.
        let raw_capacity = self.indices.buckets();
        let shifted_entries = &self.entries[index..];
        if shifted_entries.len() > raw_capacity / 2 {
            // shift all indices greater than `index`
            unsafe {
                for bucket in self.indices.iter() {
                    let i = bucket.read();
                    if i > index {
                        bucket.write(i - 1);
                    }
                }
            }
        } else {
            // find each following entry to shift its index
            for (i, entry) in (index + 1..).zip(shifted_entries) {
                let shifted_bucket = self.find_index(entry.hash, i).unwrap();
                unsafe { shifted_bucket.write(i - 1) };
            }
        }

        (index, entry.key, entry.value)
    }

    /// Remove an entry by swapping it with the last
    pub(crate) fn swap_remove_full<Q>(&mut self, hash: HashValue, key: &Q) -> Option<(usize, K, V)>
    where
        Q: ?Sized + Equivalent<K>,
    {
        match self.find_equivalent(hash, key) {
            Some(raw_bucket) => unsafe { Some(self.swap_remove_bucket(raw_bucket)) },
            None => None,
        }
    }

    /// Remove an entry by swapping it with the last
    pub(crate) fn swap_remove_index(&mut self, index: usize) -> Option<(K, V)> {
        let raw_bucket = match self.entries.get(index) {
            Some(entry) => self.find_index(entry.hash, index).unwrap(),
            None => return None,
        };
        unsafe {
            let (_, key, value) = self.swap_remove_bucket(raw_bucket);
            Some((key, value))
        }
    }

    /// Remove an entry by swapping it with the last
    ///
    /// Safety: The caller must pass a live `raw_bucket`.
    #[allow(unused_unsafe)]
    unsafe fn swap_remove_bucket(&mut self, raw_bucket: RawBucket) -> (usize, K, V) {
        // use swap_remove, but then we need to update the index that points
        // to the other entry that has to move
        let index = unsafe { self.indices.remove(raw_bucket) };
        let entry = self.entries.swap_remove(index);

        // correct index that points to the entry that had to swap places
        if let Some(entry) = self.entries.get(index) {
            // was not last element
            // examine new element in `index` and find it in indices
            let last = self.entries.len();
            let swapped_bucket = self.find_index(entry.hash, last).unwrap();
            unsafe { swapped_bucket.write(index) };
        }

        (index, entry.key, entry.value)
    }

    pub(crate) fn reverse(&mut self) {
        self.entries.reverse();

        // No need to save hash indices, can easily calculate what they should
        // be, given that this is an in-place reversal.
        let len = self.entries.len();
        unsafe {
            for raw_bucket in self.indices.iter() {
                let i = raw_bucket.read();
                raw_bucket.write(len - i - 1);
            }
        }
    }
}

/// A view into an occupied entry in a `IndexMap`.
/// It is part of the [`Entry`] enum.
///
/// [`Entry`]: enum.Entry.html
// SAFETY: The lifetime of the map reference also constrains the raw bucket,
// which is essentially a raw pointer into the map indices.
pub struct OccupiedEntry<'a, K, V> {
    map: &'a mut IndexMapCore<K, V>,
    raw_bucket: RawBucket,
    key: K,
}

// `hashbrown::raw::Bucket` is only `Send`, not `Sync`.
// SAFETY: `&self` only accesses the bucket to read it.
unsafe impl<K: Sync, V: Sync> Sync for OccupiedEntry<'_, K, V> {}

// The parent module also adds methods that don't threaten the unsafe encapsulation.
impl<'a, K, V> OccupiedEntry<'a, K, V> {
    pub fn key(&self) -> &K {
        &self.key
    }

    pub fn get(&self) -> &V {
        &self.map.entries[self.index()].value
    }

    pub fn get_mut(&mut self) -> &mut V {
        let index = self.index();
        &mut self.map.entries[index].value
    }

    /// Put the new key in the occupied entry's key slot
    pub(crate) fn replace_key(self) -> K {
        let index = self.index();
        let old_key = &mut self.map.entries[index].key;
        replace(old_key, self.key)
    }

    /// Return the index of the key-value pair
    #[inline]
    pub fn index(&self) -> usize {
        unsafe { self.raw_bucket.read() }
    }

    pub fn into_mut(self) -> &'a mut V {
        let index = self.index();
        &mut self.map.entries[index].value
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// Like `Vec::swap_remove`, the pair is removed by swapping it with the
    /// last element of the map and popping it off. **This perturbs
    /// the postion of what used to be the last element!**
    ///
    /// Computes in **O(1)** time (average).
    pub fn swap_remove_entry(self) -> (K, V) {
        // This is safe because it can only happen once (self is consumed)
        // and map.indices have not been modified since entry construction
        unsafe {
            let (_, key, value) = self.map.swap_remove_bucket(self.raw_bucket);
            (key, value)
        }
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// Like `Vec::remove`, the pair is removed by shifting all of the
    /// elements that follow it, preserving their relative order.
    /// **This perturbs the index of all of those elements!**
    ///
    /// Computes in **O(n)** time (average).
    pub fn shift_remove_entry(self) -> (K, V) {
        // This is safe because it can only happen once (self is consumed)
        // and map.indices have not been modified since entry construction
        unsafe {
            let (_, key, value) = self.map.shift_remove_bucket(self.raw_bucket);
            (key, value)
        }
    }
}
