//! An immutable map constructed at compile time.
use core::borrow::Borrow;
use core::ops::Index;
use core::slice;
use core::fmt;
use core::iter::IntoIterator;
use phf_shared::{self, PhfHash};
use Slice;

/// An immutable map constructed at compile time.
///
/// ## Note
///
/// The fields of this struct are public so that they may be initialized by the
/// `phf_map!` macro and code generation. They are subject to change at any
/// time and should never be accessed directly.
pub struct Map<K: 'static, V: 'static> {
    #[doc(hidden)]
    pub key: u64,
    #[doc(hidden)]
    pub disps: Slice<(u32, u32)>,
    #[doc(hidden)]
    pub entries: Slice<(K, V)>,
}

impl<K, V> fmt::Debug for Map<K, V> where K: fmt::Debug, V: fmt::Debug {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_map().entries(self.entries()).finish()
    }
}

impl<'a, K, V, T: ?Sized> Index<&'a T> for Map<K, V> where T: Eq + PhfHash, K: Borrow<T> {
    type Output = V;

    fn index(&self, k: &'a T) -> &V {
        self.get(k).expect("invalid key")
    }
}

impl<K, V> Map<K, V> {
    /// Returns true if the `Map` is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the number of entries in the `Map`.
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Determines if `key` is in the `Map`.
    pub fn contains_key<T: ?Sized>(&self, key: &T) -> bool
        where T: Eq + PhfHash,
              K: Borrow<T>
    {
        self.get(key).is_some()
    }

    /// Returns a reference to the value that `key` maps to.
    pub fn get<T: ?Sized>(&self, key: &T) -> Option<&V>
        where T: Eq + PhfHash,
              K: Borrow<T>
    {
        self.get_entry(key).map(|e| e.1)
    }

    /// Returns a reference to the map's internal static instance of the given
    /// key.
    ///
    /// This can be useful for interning schemes.
    pub fn get_key<T: ?Sized>(&self, key: &T) -> Option<&K>
        where T: Eq + PhfHash,
              K: Borrow<T>
    {
        self.get_entry(key).map(|e| e.0)
    }

    /// Like `get`, but returns both the key and the value.
    pub fn get_entry<T: ?Sized>(&self, key: &T) -> Option<(&K, &V)>
        where T: Eq + PhfHash,
              K: Borrow<T>
    {
        let hash = phf_shared::hash(key, self.key);
        let index = phf_shared::get_index(hash, &*self.disps, self.entries.len());
        let entry = &self.entries[index as usize];
        let b: &T = entry.0.borrow();
        if b == key {
            Some((&entry.0, &entry.1))
        } else {
            None
        }
    }

    /// Returns an iterator over the key/value pairs in the map.
    ///
    /// Entries are returned in an arbitrary but fixed order.
    pub fn entries<'a>(&'a self) -> Entries<'a, K, V> {
        Entries { iter: self.entries.iter() }
    }

    /// Returns an iterator over the keys in the map.
    ///
    /// Keys are returned in an arbitrary but fixed order.
    pub fn keys<'a>(&'a self) -> Keys<'a, K, V> {
        Keys { iter: self.entries() }
    }

    /// Returns an iterator over the values in the map.
    ///
    /// Values are returned in an arbitrary but fixed order.
    pub fn values<'a>(&'a self) -> Values<'a, K, V> {
        Values { iter: self.entries() }
    }
}

impl<'a, K, V> IntoIterator for &'a Map<K, V> {
    type Item = (&'a K, &'a V);
    type IntoIter = Entries<'a, K, V>;

    fn into_iter(self) -> Entries<'a, K, V> {
        self.entries()
    }
}

/// An iterator over the key/value pairs in a `Map`.
pub struct Entries<'a, K: 'a, V: 'a> {
    iter: slice::Iter<'a, (K, V)>,
}

impl<'a, K, V> Iterator for Entries<'a, K, V> {
    type Item = (&'a K, &'a V);

    fn next(&mut self) -> Option<(&'a K, &'a V)> {
        self.iter.next().map(|&(ref k, ref v)| (k, v))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Entries<'a, K, V> {
    fn next_back(&mut self) -> Option<(&'a K, &'a V)> {
        self.iter.next_back().map(|e| (&e.0, &e.1))
    }
}

impl<'a, K, V> ExactSizeIterator for Entries<'a, K, V> {}

/// An iterator over the keys in a `Map`.
pub struct Keys<'a, K: 'a, V: 'a> {
    iter: Entries<'a, K, V>,
}

impl<'a, K, V> Iterator for Keys<'a, K, V> {
    type Item = &'a K;

    fn next(&mut self) -> Option<&'a K> {
        self.iter.next().map(|e| e.0)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Keys<'a, K, V> {
    fn next_back(&mut self) -> Option<&'a K> {
        self.iter.next_back().map(|e| e.0)
    }
}

impl<'a, K, V> ExactSizeIterator for Keys<'a, K, V> {}

/// An iterator over the values in a `Map`.
pub struct Values<'a, K: 'a, V: 'a> {
    iter: Entries<'a, K, V>,
}

impl<'a, K, V> Iterator for Values<'a, K, V> {
    type Item = &'a V;

    fn next(&mut self) -> Option<&'a V> {
        self.iter.next().map(|e| e.1)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a, K, V> DoubleEndedIterator for Values<'a, K, V> {
    fn next_back(&mut self) -> Option<&'a V> {
        self.iter.next_back().map(|e| e.1)
    }
}

impl<'a, K, V> ExactSizeIterator for Values<'a, K, V> {}
