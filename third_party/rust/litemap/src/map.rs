// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::store::*;
use alloc::borrow::Borrow;
use alloc::vec::Vec;
use core::cmp::Ordering;
use core::iter::FromIterator;
use core::marker::PhantomData;
use core::mem;
use core::ops::{Index, IndexMut};

/// A simple "flat" map based on a sorted vector
///
/// See the [module level documentation][super] for why one should use this.
///
/// The API is roughly similar to that of [`std::collections::BTreeMap`].
#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[cfg_attr(feature = "yoke", derive(yoke::Yokeable))]
pub struct LiteMap<K: ?Sized, V: ?Sized, S = alloc::vec::Vec<(K, V)>> {
    pub(crate) values: S,
    pub(crate) _key_type: PhantomData<K>,
    pub(crate) _value_type: PhantomData<V>,
}

impl<K, V> LiteMap<K, V> {
    /// Construct a new [`LiteMap`] backed by Vec
    pub const fn new_vec() -> Self {
        Self {
            values: alloc::vec::Vec::new(),
            _key_type: PhantomData,
            _value_type: PhantomData,
        }
    }
}

impl<K, V, S> LiteMap<K, V, S> {
    /// Construct a new [`LiteMap`] using the given values
    ///
    /// The store must be sorted and have no duplicate keys.
    pub const fn from_sorted_store_unchecked(values: S) -> Self {
        Self {
            values,
            _key_type: PhantomData,
            _value_type: PhantomData,
        }
    }
}

impl<K, V> LiteMap<K, V, Vec<(K, V)>> {
    /// Convert a [`LiteMap`] into a sorted `Vec<(K, V)>`.
    #[inline]
    pub fn into_tuple_vec(self) -> Vec<(K, V)> {
        self.values
    }
}

impl<K: ?Sized, V: ?Sized, S> LiteMap<K, V, S>
where
    S: StoreConstEmpty<K, V>,
{
    /// Create a new empty [`LiteMap`]
    pub const fn new() -> Self {
        Self {
            values: S::EMPTY,
            _key_type: PhantomData,
            _value_type: PhantomData,
        }
    }
}

impl<K: ?Sized, V: ?Sized, S> LiteMap<K, V, S>
where
    S: Store<K, V>,
{
    /// The number of elements in the [`LiteMap`]
    pub fn len(&self) -> usize {
        self.values.lm_len()
    }

    /// Whether the [`LiteMap`] is empty
    pub fn is_empty(&self) -> bool {
        self.values.lm_is_empty()
    }

    /// Get the key-value pair residing at a particular index
    ///
    /// In most cases, prefer [`LiteMap::get()`] over this method.
    #[inline]
    pub fn get_indexed(&self, index: usize) -> Option<(&K, &V)> {
        self.values.lm_get(index)
    }
}

impl<K: ?Sized, V: ?Sized, S> LiteMap<K, V, S>
where
    K: Ord,
    S: Store<K, V>,
{
    /// Get the value associated with `key`, if it exists.
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// assert_eq!(map.get(&1), Some(&"one"));
    /// assert_eq!(map.get(&3), None);
    /// ```
    pub fn get<Q: ?Sized>(&self, key: &Q) -> Option<&V>
    where
        K: Borrow<Q>,
        Q: Ord,
    {
        match self.find_index(key) {
            #[allow(clippy::unwrap_used)] // find_index returns a valid index
            Ok(found) => Some(self.values.lm_get(found).unwrap().1),
            Err(_) => None,
        }
    }

    /// Binary search the map with `predicate` to find a key, returning the value.
    pub fn get_by(&self, predicate: impl FnMut(&K) -> Ordering) -> Option<&V> {
        let index = self.values.lm_binary_search_by(predicate).ok()?;
        self.values.lm_get(index).map(|(_, v)| v)
    }

    /// Returns whether `key` is contained in this map
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// assert!(map.contains_key(&1));
    /// assert!(!map.contains_key(&3));
    /// ```
    pub fn contains_key<Q: ?Sized>(&self, key: &Q) -> bool
    where
        K: Borrow<Q>,
        Q: Ord,
    {
        self.find_index(key).is_ok()
    }

    /// Get the lowest-rank key/value pair from the `LiteMap`, if it exists.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map: LiteMap<i32, &str, Vec<_>> =
    ///     LiteMap::from_iter([(1, "uno"), (3, "tres")].into_iter());
    ///
    /// assert_eq!(map.first(), Some((&1, &"uno")));
    /// ```
    #[inline]
    pub fn first(&self) -> Option<(&K, &V)> {
        self.values.lm_get(0).map(|(k, v)| (k, v))
    }

    /// Get the highest-rank key/value pair from the `LiteMap`, if it exists.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map: LiteMap<i32, &str, Vec<_>> =
    ///     LiteMap::from_iter([(1, "uno"), (3, "tres")].into_iter());
    ///
    /// assert_eq!(map.last(), Some((&3, &"tres")));
    /// ```
    #[inline]
    pub fn last(&self) -> Option<(&K, &V)> {
        self.values.lm_get(self.len() - 1).map(|(k, v)| (k, v))
    }

    /// Obtain the index for a given key, or if the key is not found, the index
    /// at which it would be inserted.
    ///
    /// (The return value works equivalently to [`slice::binary_search_by()`])
    ///
    /// The indices returned can be used with [`Self::get_indexed()`]. Prefer using
    /// [`Self::get()`] directly where possible.
    #[inline]
    pub fn find_index<Q: ?Sized>(&self, key: &Q) -> Result<usize, usize>
    where
        K: Borrow<Q>,
        Q: Ord,
    {
        self.values.lm_binary_search_by(|k| k.borrow().cmp(key))
    }
}

impl<K, V, S> LiteMap<K, V, S>
where
    S: StoreMut<K, V>,
{
    /// Construct a new [`LiteMap`] with a given capacity
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            values: S::lm_with_capacity(capacity),
            _key_type: PhantomData,
            _value_type: PhantomData,
        }
    }

    /// Remove all elements from the [`LiteMap`]
    pub fn clear(&mut self) {
        self.values.lm_clear()
    }

    /// Reserve capacity for `additional` more elements to be inserted into
    /// the [`LiteMap`] to avoid frequent reallocations.
    ///
    /// See [`Vec::reserve()`] for more information.
    ///
    /// [`Vec::reserve()`]: alloc::vec::Vec::reserve
    pub fn reserve(&mut self, additional: usize) {
        self.values.lm_reserve(additional)
    }
}

impl<K, V, S> LiteMap<K, V, S>
where
    K: Ord,
    S: StoreMut<K, V>,
{
    /// Get the value associated with `key`, if it exists, as a mutable reference.
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// if let Some(mut v) = map.get_mut(&1) {
    ///     *v = "uno";
    /// }
    /// assert_eq!(map.get(&1), Some(&"uno"));
    /// ```
    pub fn get_mut<Q: ?Sized>(&mut self, key: &Q) -> Option<&mut V>
    where
        K: Borrow<Q>,
        Q: Ord,
    {
        match self.find_index(key) {
            #[allow(clippy::unwrap_used)] // find_index returns a valid index
            Ok(found) => Some(self.values.lm_get_mut(found).unwrap().1),
            Err(_) => None,
        }
    }

    /// Appends `value` with `key` to the end of the underlying vector, returning
    /// `key` and `value` _if it failed_. Useful for extending with an existing
    /// sorted list.
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// assert!(map.try_append(1, "uno").is_none());
    /// assert!(map.try_append(3, "tres").is_none());
    ///
    /// assert!(
    ///     matches!(map.try_append(3, "tres-updated"), Some((3, "tres-updated"))),
    ///     "append duplicate of last key",
    /// );
    ///
    /// assert!(
    ///     matches!(map.try_append(2, "dos"), Some((2, "dos"))),
    ///     "append out of order"
    /// );
    ///
    /// assert_eq!(map.get(&1), Some(&"uno"));
    ///
    /// // contains the original value for the key: 3
    /// assert_eq!(map.get(&3), Some(&"tres"));
    ///
    /// // not appended since it wasn't in order
    /// assert_eq!(map.get(&2), None);
    /// ```
    #[must_use]
    pub fn try_append(&mut self, key: K, value: V) -> Option<(K, V)> {
        if let Some(last) = self.values.lm_last() {
            if last.0 >= &key {
                return Some((key, value));
            }
        }

        self.values.lm_push(key, value);
        None
    }

    /// Insert `value` with `key`, returning the existing value if it exists.
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// assert_eq!(map.get(&1), Some(&"one"));
    /// assert_eq!(map.get(&3), None);
    /// ```
    pub fn insert(&mut self, key: K, value: V) -> Option<V> {
        self.insert_save_key(key, value).map(|(_, v)| v)
    }

    /// Version of [`Self::insert()`] that returns both the key and the old value.
    fn insert_save_key(&mut self, key: K, value: V) -> Option<(K, V)> {
        match self.values.lm_binary_search_by(|k| k.cmp(&key)) {
            #[allow(clippy::unwrap_used)] // Index came from binary_search
            Ok(found) => Some((
                key,
                mem::replace(self.values.lm_get_mut(found).unwrap().1, value),
            )),
            Err(ins) => {
                self.values.lm_insert(ins, key, value);
                None
            }
        }
    }

    /// Attempts to insert a unique entry into the map.
    ///
    /// If `key` is not already in the map, inserts it with the corresponding `value`
    /// and returns `None`.
    ///
    /// If `key` is already in the map, no change is made to the map, and the key and value
    /// are returned back to the caller.
    ///
    /// ```
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(3, "three");
    ///
    /// // 2 is not yet in the map...
    /// assert_eq!(map.try_insert(2, "two"), None);
    /// assert_eq!(map.len(), 3);
    ///
    /// // ...but now it is.
    /// assert_eq!(map.try_insert(2, "TWO"), Some((2, "TWO")));
    /// assert_eq!(map.len(), 3);
    /// ```
    pub fn try_insert(&mut self, key: K, value: V) -> Option<(K, V)> {
        match self.values.lm_binary_search_by(|k| k.cmp(&key)) {
            Ok(_) => Some((key, value)),
            Err(ins) => {
                self.values.lm_insert(ins, key, value);
                None
            }
        }
    }

    /// Remove the value at `key`, returning it if it exists.
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// assert_eq!(map.remove(&1), Some("one"));
    /// assert_eq!(map.get(&1), None);
    /// ```
    pub fn remove<Q: ?Sized>(&mut self, key: &Q) -> Option<V>
    where
        K: Borrow<Q>,
        Q: Ord,
    {
        match self.values.lm_binary_search_by(|k| k.borrow().cmp(key)) {
            Ok(found) => Some(self.values.lm_remove(found).1),
            Err(_) => None,
        }
    }
}

impl<'a, K: 'a, V: 'a, S> LiteMap<K, V, S>
where
    K: Ord,
    S: StoreIterableMut<'a, K, V> + StoreFromIterator<K, V>,
{
    /// Insert all elements from `other` into this `LiteMap`.
    ///
    /// If `other` contains keys that already exist in `self`, the values in `other` replace the
    /// corresponding ones in `self`, and the rejected items from `self` are returned as a new
    /// `LiteMap`. Otherwise, `None` is returned.
    ///
    /// The implementation of this function is optimized if `self` and `other` have no overlap.
    ///
    /// # Examples
    ///
    /// ```
    /// use litemap::LiteMap;
    ///
    /// let mut map1 = LiteMap::new_vec();
    /// map1.insert(1, "one");
    /// map1.insert(2, "two");
    ///
    /// let mut map2 = LiteMap::new_vec();
    /// map2.insert(2, "TWO");
    /// map2.insert(4, "FOUR");
    ///
    /// let leftovers = map1.extend_from_litemap(map2);
    ///
    /// assert_eq!(map1.len(), 3);
    /// assert_eq!(map1.get(&1), Some("one").as_ref());
    /// assert_eq!(map1.get(&2), Some("TWO").as_ref());
    /// assert_eq!(map1.get(&4), Some("FOUR").as_ref());
    ///
    /// let map3 = leftovers.expect("Duplicate keys");
    /// assert_eq!(map3.len(), 1);
    /// assert_eq!(map3.get(&2), Some("two").as_ref());
    /// ```
    pub fn extend_from_litemap(&mut self, other: Self) -> Option<Self> {
        if self.is_empty() {
            self.values = other.values;
            return None;
        }
        if other.is_empty() {
            return None;
        }
        if self.last().map(|(k, _)| k) < other.first().map(|(k, _)| k) {
            // append other to self
            self.values.lm_extend_end(other.values);
            None
        } else if self.first().map(|(k, _)| k) > other.last().map(|(k, _)| k) {
            // prepend other to self
            self.values.lm_extend_start(other.values);
            None
        } else {
            // insert every element
            let leftover_tuples = other
                .values
                .lm_into_iter()
                .filter_map(|(k, v)| self.insert_save_key(k, v))
                .collect();
            let ret = LiteMap {
                values: leftover_tuples,
                _key_type: PhantomData,
                _value_type: PhantomData,
            };
            if ret.is_empty() {
                None
            } else {
                Some(ret)
            }
        }
    }
}

impl<K, V, S> Default for LiteMap<K, V, S>
where
    S: Store<K, V> + Default,
{
    fn default() -> Self {
        Self {
            values: S::default(),
            _key_type: PhantomData,
            _value_type: PhantomData,
        }
    }
}
impl<K, V, S> Index<&'_ K> for LiteMap<K, V, S>
where
    K: Ord,
    S: Store<K, V>,
{
    type Output = V;
    fn index(&self, key: &K) -> &V {
        #[allow(clippy::panic)] // documented
        match self.get(key) {
            Some(v) => v,
            None => panic!("no entry found for key"),
        }
    }
}
impl<K, V, S> IndexMut<&'_ K> for LiteMap<K, V, S>
where
    K: Ord,
    S: StoreMut<K, V>,
{
    fn index_mut(&mut self, key: &K) -> &mut V {
        #[allow(clippy::panic)] // documented
        match self.get_mut(key) {
            Some(v) => v,
            None => panic!("no entry found for key"),
        }
    }
}
impl<K, V, S> FromIterator<(K, V)> for LiteMap<K, V, S>
where
    K: Ord,
    S: StoreFromIterable<K, V>,
{
    fn from_iter<I: IntoIterator<Item = (K, V)>>(iter: I) -> Self {
        let values = S::lm_sort_from_iter(iter);
        Self::from_sorted_store_unchecked(values)
    }
}

impl<'a, K: 'a, V: 'a, S> LiteMap<K, V, S>
where
    S: StoreIterable<'a, K, V>,
{
    /// Produce an ordered iterator over key-value pairs
    pub fn iter(&'a self) -> impl Iterator<Item = (&'a K, &'a V)> + DoubleEndedIterator {
        self.values.lm_iter()
    }

    /// Produce an ordered iterator over keys
    pub fn iter_keys(&'a self) -> impl Iterator<Item = &'a K> + DoubleEndedIterator {
        self.values.lm_iter().map(|val| val.0)
    }

    /// Produce an iterator over values, ordered by their keys
    pub fn iter_values(&'a self) -> impl Iterator<Item = &'a V> + DoubleEndedIterator {
        self.values.lm_iter().map(|val| val.1)
    }
}

impl<'a, K: 'a, V: 'a, S> LiteMap<K, V, S>
where
    S: StoreIterableMut<'a, K, V>,
{
    /// Produce an ordered mutable iterator over key-value pairs
    pub fn iter_mut(
        &'a mut self,
    ) -> impl Iterator<Item = (&'a K, &'a mut V)> + DoubleEndedIterator {
        self.values.lm_iter_mut()
    }
}

impl<K, V, S> LiteMap<K, V, S>
where
    S: StoreMut<K, V>,
{
    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements such that `f((&k, &v))` returns `false`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use litemap::LiteMap;
    ///
    /// let mut map = LiteMap::new_vec();
    /// map.insert(1, "one");
    /// map.insert(2, "two");
    /// map.insert(3, "three");
    ///
    /// // Retain elements with odd keys
    /// map.retain(|k, _| k % 2 == 1);
    ///
    /// assert_eq!(map.get(&1), Some(&"one"));
    /// assert_eq!(map.get(&2), None);
    /// ```
    #[inline]
    pub fn retain<F>(&mut self, predicate: F)
    where
        F: FnMut(&K, &V) -> bool,
    {
        self.values.lm_retain(predicate)
    }
}

#[cfg(test)]
mod test {
    use crate::LiteMap;

    #[test]
    fn from_iterator() {
        let mut expected = LiteMap::with_capacity(4);
        expected.insert(1, "updated-one");
        expected.insert(2, "original-two");
        expected.insert(3, "original-three");
        expected.insert(4, "updated-four");

        let actual = vec![
            (1, "original-one"),
            (2, "original-two"),
            (4, "original-four"),
            (4, "updated-four"),
            (1, "updated-one"),
            (3, "original-three"),
        ]
        .into_iter()
        .collect::<LiteMap<_, _>>();

        assert_eq!(expected, actual);
    }
    fn make_13() -> LiteMap<usize, &'static str> {
        let mut result = LiteMap::new();
        result.insert(1, "one");
        result.insert(3, "three");
        result
    }

    fn make_24() -> LiteMap<usize, &'static str> {
        let mut result = LiteMap::new();
        result.insert(2, "TWO");
        result.insert(4, "FOUR");
        result
    }

    fn make_46() -> LiteMap<usize, &'static str> {
        let mut result = LiteMap::new();
        result.insert(4, "four");
        result.insert(6, "six");
        result
    }

    #[test]
    fn extend_from_litemap_append() {
        let mut map = LiteMap::new();
        map.extend_from_litemap(make_13())
            .ok_or(())
            .expect_err("Append to empty map");
        map.extend_from_litemap(make_46())
            .ok_or(())
            .expect_err("Append to lesser map");
        assert_eq!(map.len(), 4);
    }

    #[test]
    fn extend_from_litemap_prepend() {
        let mut map = LiteMap::new();
        map.extend_from_litemap(make_46())
            .ok_or(())
            .expect_err("Prepend to empty map");
        map.extend_from_litemap(make_13())
            .ok_or(())
            .expect_err("Prepend to lesser map");
        assert_eq!(map.len(), 4);
    }

    #[test]
    fn extend_from_litemap_insert() {
        let mut map = LiteMap::new();
        map.extend_from_litemap(make_13())
            .ok_or(())
            .expect_err("Append to empty map");
        map.extend_from_litemap(make_24())
            .ok_or(())
            .expect_err("Insert with no conflict");
        map.extend_from_litemap(make_46())
            .ok_or(())
            .expect("Insert with conflict");
        assert_eq!(map.len(), 5);
    }
}
