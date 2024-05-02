#![allow(unsafe_code)]
//! This module encapsulates the `unsafe` access to `hashbrown::raw::RawTable`,
//! mostly in dealing with its bucket "pointers".

use super::{equivalent, get_hash, Bucket, HashValue, IndexMapCore};
use hashbrown::raw::RawTable;

type RawBucket = hashbrown::raw::Bucket<usize>;

/// Inserts many entries into a raw table without reallocating.
///
/// ***Panics*** if there is not sufficient capacity already.
pub(super) fn insert_bulk_no_grow<K, V>(indices: &mut RawTable<usize>, entries: &[Bucket<K, V>]) {
    assert!(indices.capacity() - indices.len() >= entries.len());
    for entry in entries {
        // SAFETY: we asserted that sufficient capacity exists for all entries.
        unsafe {
            indices.insert_no_grow(entry.hash.get(), indices.len());
        }
    }
}

#[cfg(feature = "test_debug")]
pub(super) struct DebugIndices<'a>(pub &'a RawTable<usize>);

#[cfg(feature = "test_debug")]
impl core::fmt::Debug for DebugIndices<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        // SAFETY: we're not letting any of the buckets escape this function
        let indices = unsafe { self.0.iter().map(|raw_bucket| *raw_bucket.as_ref()) };
        f.debug_list().entries(indices).finish()
    }
}

impl<K, V> IndexMapCore<K, V> {
    /// Sweep the whole table to erase indices start..end
    pub(super) fn erase_indices_sweep(&mut self, start: usize, end: usize) {
        // SAFETY: we're not letting any of the buckets escape this function
        unsafe {
            let offset = end - start;
            for bucket in self.indices.iter() {
                let i = bucket.as_mut();
                if *i >= end {
                    *i -= offset;
                } else if *i >= start {
                    self.indices.erase(bucket);
                }
            }
        }
    }

    /// Search for a key in the table and return `Ok(entry_index)` if found.
    /// Otherwise, insert the key and return `Err(new_index)`.
    ///
    /// Note that hashbrown may resize the table to reserve space for insertion,
    /// even before checking if it's already present, so this is somewhat biased
    /// towards new items.
    pub(crate) fn find_or_insert(&mut self, hash: HashValue, key: &K) -> Result<usize, usize>
    where
        K: Eq,
    {
        let hash = hash.get();
        let eq = equivalent(key, &self.entries);
        let hasher = get_hash(&self.entries);
        // SAFETY: We're not mutating between find and read/insert.
        unsafe {
            match self.indices.find_or_find_insert_slot(hash, eq, hasher) {
                Ok(raw_bucket) => Ok(*raw_bucket.as_ref()),
                Err(slot) => {
                    let index = self.indices.len();
                    self.indices.insert_in_slot(hash, slot, index);
                    Err(index)
                }
            }
        }
    }

    pub(super) fn raw_entry(
        &mut self,
        hash: HashValue,
        mut is_match: impl FnMut(&K) -> bool,
    ) -> Result<RawTableEntry<'_, K, V>, &mut Self> {
        let entries = &*self.entries;
        let eq = move |&i: &usize| is_match(&entries[i].key);
        match self.indices.find(hash.get(), eq) {
            // SAFETY: The entry is created with a live raw bucket, at the same time
            // we have a &mut reference to the map, so it can not be modified further.
            Some(raw_bucket) => Ok(RawTableEntry {
                map: self,
                raw_bucket,
            }),
            None => Err(self),
        }
    }

    pub(super) fn indices_mut(&mut self) -> impl Iterator<Item = &mut usize> {
        // SAFETY: we're not letting any of the buckets escape this function,
        // only the item references that are appropriately bound to `&mut self`.
        unsafe { self.indices.iter().map(|bucket| bucket.as_mut()) }
    }
}

/// A view into an occupied raw entry in an `IndexMap`.
// SAFETY: The lifetime of the map reference also constrains the raw bucket,
// which is essentially a raw pointer into the map indices.
pub(super) struct RawTableEntry<'a, K, V> {
    map: &'a mut IndexMapCore<K, V>,
    raw_bucket: RawBucket,
}

// `hashbrown::raw::Bucket` is only `Send`, not `Sync`.
// SAFETY: `&self` only accesses the bucket to read it.
unsafe impl<K: Sync, V: Sync> Sync for RawTableEntry<'_, K, V> {}

impl<'a, K, V> RawTableEntry<'a, K, V> {
    /// Return the index of the key-value pair
    #[inline]
    pub(super) fn index(&self) -> usize {
        // SAFETY: we have `&mut map` keeping the bucket stable
        unsafe { *self.raw_bucket.as_ref() }
    }

    #[inline]
    pub(super) fn bucket(&self) -> &Bucket<K, V> {
        &self.map.entries[self.index()]
    }

    #[inline]
    pub(super) fn bucket_mut(&mut self) -> &mut Bucket<K, V> {
        let index = self.index();
        &mut self.map.entries[index]
    }

    #[inline]
    pub(super) fn into_bucket(self) -> &'a mut Bucket<K, V> {
        let index = self.index();
        &mut self.map.entries[index]
    }

    /// Remove the index from indices, leaving the actual entries to the caller.
    pub(super) fn remove_index(self) -> (&'a mut IndexMapCore<K, V>, usize) {
        // SAFETY: This is safe because it can only happen once (self is consumed)
        // and map.indices have not been modified since entry construction
        let (index, _slot) = unsafe { self.map.indices.remove(self.raw_bucket) };
        (self.map, index)
    }

    /// Take no action, just return the index and the original map reference.
    pub(super) fn into_inner(self) -> (&'a mut IndexMapCore<K, V>, usize) {
        let index = self.index();
        (self.map, index)
    }
}
