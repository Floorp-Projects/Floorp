//! This is the core implementation that doesn't depend on the hasher at all.
//!
//! The methods of `IndexMapCore` don't use any Hash properties of K.
//!
//! It's cleaner to separate them out, then the compiler checks that we are not
//! using Hash at all in these methods.
//!
//! However, we should probably not let this show in the public API or docs.

mod raw;

use hashbrown::raw::RawTable;

use crate::vec::{Drain, Vec};
use core::cmp;
use core::fmt;
use core::mem::replace;
use core::ops::RangeBounds;

use crate::equivalent::Equivalent;
use crate::util::{enumerate, simplify_range};
use crate::{Bucket, Entries, HashValue};

/// Core of the map that does not depend on S
pub(crate) struct IndexMapCore<K, V> {
    /// indices mapping from the entry hash to its index.
    indices: RawTable<usize>,
    /// entries is a dense vec of entries in their order.
    entries: Vec<Bucket<K, V>>,
}

#[inline(always)]
fn get_hash<K, V>(entries: &[Bucket<K, V>]) -> impl Fn(&usize) -> u64 + '_ {
    move |&i| entries[i].hash.get()
}

impl<K, V> Clone for IndexMapCore<K, V>
where
    K: Clone,
    V: Clone,
{
    fn clone(&self) -> Self {
        let indices = self.indices.clone();
        let mut entries = Vec::with_capacity(indices.capacity());
        entries.clone_from(&self.entries);
        IndexMapCore { indices, entries }
    }

    fn clone_from(&mut self, other: &Self) {
        let hasher = get_hash(&other.entries);
        self.indices.clone_from_with_hasher(&other.indices, hasher);
        if self.entries.capacity() < other.entries.len() {
            // If we must resize, match the indices capacity
            self.reserve_entries();
        }
        self.entries.clone_from(&other.entries);
    }
}

impl<K, V> fmt::Debug for IndexMapCore<K, V>
where
    K: fmt::Debug,
    V: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("IndexMapCore")
            .field("indices", &raw::DebugIndices(&self.indices))
            .field("entries", &self.entries)
            .finish()
    }
}

impl<K, V> Entries for IndexMapCore<K, V> {
    type Entry = Bucket<K, V>;

    #[inline]
    fn into_entries(self) -> Vec<Self::Entry> {
        self.entries
    }

    #[inline]
    fn as_entries(&self) -> &[Self::Entry] {
        &self.entries
    }

    #[inline]
    fn as_entries_mut(&mut self) -> &mut [Self::Entry] {
        &mut self.entries
    }

    fn with_entries<F>(&mut self, f: F)
    where
        F: FnOnce(&mut [Self::Entry]),
    {
        f(&mut self.entries);
        self.rebuild_hash_table();
    }
}

impl<K, V> IndexMapCore<K, V> {
    #[inline]
    pub(crate) fn new() -> Self {
        IndexMapCore {
            indices: RawTable::new(),
            entries: Vec::new(),
        }
    }

    #[inline]
    pub(crate) fn with_capacity(n: usize) -> Self {
        IndexMapCore {
            indices: RawTable::with_capacity(n),
            entries: Vec::with_capacity(n),
        }
    }

    #[inline]
    pub(crate) fn len(&self) -> usize {
        self.indices.len()
    }

    #[inline]
    pub(crate) fn capacity(&self) -> usize {
        cmp::min(self.indices.capacity(), self.entries.capacity())
    }

    pub(crate) fn clear(&mut self) {
        self.indices.clear();
        self.entries.clear();
    }

    pub(crate) fn drain<R>(&mut self, range: R) -> Drain<'_, Bucket<K, V>>
    where
        R: RangeBounds<usize>,
    {
        let range = simplify_range(range, self.entries.len());
        self.erase_indices(range.start, range.end);
        self.entries.drain(range)
    }

    /// Reserve capacity for `additional` more key-value pairs.
    pub(crate) fn reserve(&mut self, additional: usize) {
        self.indices.reserve(additional, get_hash(&self.entries));
        self.reserve_entries();
    }

    /// Reserve entries capacity to match the indices
    fn reserve_entries(&mut self) {
        let additional = self.indices.capacity() - self.entries.len();
        self.entries.reserve_exact(additional);
    }

    /// Shrink the capacity of the map as much as possible.
    pub(crate) fn shrink_to_fit(&mut self) {
        self.indices.shrink_to(0, get_hash(&self.entries));
        self.entries.shrink_to_fit();
    }

    /// Remove the last key-value pair
    pub(crate) fn pop(&mut self) -> Option<(K, V)> {
        if let Some(entry) = self.entries.pop() {
            let last = self.entries.len();
            self.erase_index(entry.hash, last);
            Some((entry.key, entry.value))
        } else {
            None
        }
    }

    /// Append a key-value pair, *without* checking whether it already exists,
    /// and return the pair's new index.
    fn push(&mut self, hash: HashValue, key: K, value: V) -> usize {
        let i = self.entries.len();
        self.indices.insert(hash.get(), i, get_hash(&self.entries));
        if i == self.entries.capacity() {
            // Reserve our own capacity synced to the indices,
            // rather than letting `Vec::push` just double it.
            self.reserve_entries();
        }
        self.entries.push(Bucket { hash, key, value });
        i
    }

    pub(crate) fn insert_full(&mut self, hash: HashValue, key: K, value: V) -> (usize, Option<V>)
    where
        K: Eq,
    {
        match self.get_index_of(hash, &key) {
            Some(i) => (i, Some(replace(&mut self.entries[i].value, value))),
            None => (self.push(hash, key, value), None),
        }
    }

    pub(crate) fn retain_in_order<F>(&mut self, mut keep: F)
    where
        F: FnMut(&mut K, &mut V) -> bool,
    {
        // Like Vec::retain in self.entries, but with mutable K and V.
        // We swap-shift all the items we want to keep, truncate the rest,
        // then rebuild the raw hash table with the new indexes.
        let len = self.entries.len();
        let mut n_deleted = 0;
        for i in 0..len {
            let will_keep = {
                let entry = &mut self.entries[i];
                keep(&mut entry.key, &mut entry.value)
            };
            if !will_keep {
                n_deleted += 1;
            } else if n_deleted > 0 {
                self.entries.swap(i - n_deleted, i);
            }
        }
        if n_deleted > 0 {
            self.entries.truncate(len - n_deleted);
            self.rebuild_hash_table();
        }
    }

    fn rebuild_hash_table(&mut self) {
        self.indices.clear();
        debug_assert!(self.indices.capacity() >= self.entries.len());
        for (i, entry) in enumerate(&self.entries) {
            // We should never have to reallocate, so there's no need for a real hasher.
            self.indices.insert_no_grow(entry.hash.get(), i);
        }
    }
}

/// Entry for an existing key-value pair or a vacant location to
/// insert one.
pub enum Entry<'a, K, V> {
    /// Existing slot with equivalent key.
    Occupied(OccupiedEntry<'a, K, V>),
    /// Vacant slot (no equivalent key in the map).
    Vacant(VacantEntry<'a, K, V>),
}

impl<'a, K, V> Entry<'a, K, V> {
    /// Computes in **O(1)** time (amortized average).
    pub fn or_insert(self, default: V) -> &'a mut V {
        match self {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => entry.insert(default),
        }
    }

    /// Computes in **O(1)** time (amortized average).
    pub fn or_insert_with<F>(self, call: F) -> &'a mut V
    where
        F: FnOnce() -> V,
    {
        match self {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => entry.insert(call()),
        }
    }

    pub fn key(&self) -> &K {
        match *self {
            Entry::Occupied(ref entry) => entry.key(),
            Entry::Vacant(ref entry) => entry.key(),
        }
    }

    /// Return the index where the key-value pair exists or will be inserted.
    pub fn index(&self) -> usize {
        match *self {
            Entry::Occupied(ref entry) => entry.index(),
            Entry::Vacant(ref entry) => entry.index(),
        }
    }

    /// Modifies the entry if it is occupied.
    pub fn and_modify<F>(self, f: F) -> Self
    where
        F: FnOnce(&mut V),
    {
        match self {
            Entry::Occupied(mut o) => {
                f(o.get_mut());
                Entry::Occupied(o)
            }
            x => x,
        }
    }

    /// Inserts a default-constructed value in the entry if it is vacant and returns a mutable
    /// reference to it. Otherwise a mutable reference to an already existent value is returned.
    ///
    /// Computes in **O(1)** time (amortized average).
    pub fn or_default(self) -> &'a mut V
    where
        V: Default,
    {
        match self {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => entry.insert(V::default()),
        }
    }
}

impl<K: fmt::Debug, V: fmt::Debug> fmt::Debug for Entry<'_, K, V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Entry::Vacant(ref v) => f.debug_tuple(stringify!(Entry)).field(v).finish(),
            Entry::Occupied(ref o) => f.debug_tuple(stringify!(Entry)).field(o).finish(),
        }
    }
}

pub use self::raw::OccupiedEntry;

// Extra methods that don't threaten the unsafe encapsulation.
impl<K, V> OccupiedEntry<'_, K, V> {
    /// Sets the value of the entry to `value`, and returns the entry's old value.
    pub fn insert(&mut self, value: V) -> V {
        replace(self.get_mut(), value)
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// **NOTE:** This is equivalent to `.swap_remove()`.
    pub fn remove(self) -> V {
        self.swap_remove()
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// Like `Vec::swap_remove`, the pair is removed by swapping it with the
    /// last element of the map and popping it off. **This perturbs
    /// the postion of what used to be the last element!**
    ///
    /// Computes in **O(1)** time (average).
    pub fn swap_remove(self) -> V {
        self.swap_remove_entry().1
    }

    /// Remove the key, value pair stored in the map for this entry, and return the value.
    ///
    /// Like `Vec::remove`, the pair is removed by shifting all of the
    /// elements that follow it, preserving their relative order.
    /// **This perturbs the index of all of those elements!**
    ///
    /// Computes in **O(n)** time (average).
    pub fn shift_remove(self) -> V {
        self.shift_remove_entry().1
    }

    /// Remove and return the key, value pair stored in the map for this entry
    ///
    /// **NOTE:** This is equivalent to `.swap_remove_entry()`.
    pub fn remove_entry(self) -> (K, V) {
        self.swap_remove_entry()
    }
}

impl<K: fmt::Debug, V: fmt::Debug> fmt::Debug for OccupiedEntry<'_, K, V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct(stringify!(OccupiedEntry))
            .field("key", self.key())
            .field("value", self.get())
            .finish()
    }
}

/// A view into a vacant entry in a `IndexMap`.
/// It is part of the [`Entry`] enum.
///
/// [`Entry`]: enum.Entry.html
pub struct VacantEntry<'a, K, V> {
    map: &'a mut IndexMapCore<K, V>,
    hash: HashValue,
    key: K,
}

impl<'a, K, V> VacantEntry<'a, K, V> {
    pub fn key(&self) -> &K {
        &self.key
    }

    pub fn into_key(self) -> K {
        self.key
    }

    /// Return the index where the key-value pair will be inserted.
    pub fn index(&self) -> usize {
        self.map.len()
    }

    pub fn insert(self, value: V) -> &'a mut V {
        let i = self.map.push(self.hash, self.key, value);
        &mut self.map.entries[i].value
    }
}

impl<K: fmt::Debug, V> fmt::Debug for VacantEntry<'_, K, V> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple(stringify!(VacantEntry))
            .field(self.key())
            .finish()
    }
}

#[test]
fn assert_send_sync() {
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<IndexMapCore<i32, i32>>();
    assert_send_sync::<Entry<'_, i32, i32>>();
}
