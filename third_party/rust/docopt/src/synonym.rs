use std::collections::HashMap;
use std::collections::hash_map::{Iter, Keys};
use std::fmt::Debug;
use std::hash::Hash;
use std::iter::{FromIterator, IntoIterator};
use std::mem;

#[derive(Clone)]
pub struct SynonymMap<K, V> {
    vals: HashMap<K, V>,
    syns: HashMap<K, K>,
}

impl<K: Eq + Hash, V> SynonymMap<K, V> {
    pub fn new() -> SynonymMap<K, V> {
        SynonymMap {
            vals: HashMap::new(),
            syns: HashMap::new(),
        }
    }

    pub fn insert_synonym(&mut self, from: K, to: K) -> bool {
        assert!(self.vals.contains_key(&to));
        self.syns.insert(from, to).is_none()
    }

    pub fn keys(&self) -> Keys<K, V> {
        self.vals.keys()
    }

    pub fn iter(&self) -> Iter<K, V> {
        self.vals.iter()
    }

    pub fn synonyms(&self) -> Iter<K, K> {
        self.syns.iter()
    }

    pub fn find(&self, k: &K) -> Option<&V> {
        self.with_key(k, |k| self.vals.get(k))
    }

    pub fn contains_key(&self, k: &K) -> bool {
        self.with_key(k, |k| self.vals.contains_key(k))
    }

    pub fn len(&self) -> usize {
        self.vals.len()
    }

    fn with_key<T, F>(&self, k: &K, with: F) -> T where F: FnOnce(&K) -> T {
        if self.syns.contains_key(k) {
            with(&self.syns[k])
        } else {
            with(k)
        }
    }
}

impl<K: Eq + Hash + Clone, V> SynonymMap<K, V> {
    pub fn resolve(&self, k: &K) -> K {
        self.with_key(k, |k| k.clone())
    }

    pub fn get<'a>(&'a self, k: &K) -> &'a V {
        self.find(k).unwrap()
    }

    pub fn find_mut<'a>(&'a mut self, k: &K) -> Option<&'a mut V> {
        if self.syns.contains_key(k) {
            self.vals.get_mut(&self.syns[k])
        } else {
            self.vals.get_mut(k)
        }
    }

    pub fn swap(&mut self, k: K, mut new: V) -> Option<V> {
        if self.syns.contains_key(&k) {
            let old = self.vals.get_mut(&k).unwrap();
            mem::swap(old, &mut new);
            Some(new)
        } else {
            self.vals.insert(k, new)
        }
    }

    pub fn insert(&mut self, k: K, v: V) -> bool {
        self.swap(k, v).is_none()
    }
}

impl<K: Eq + Hash + Clone, V> FromIterator<(K, V)> for SynonymMap<K, V> {
    fn from_iter<T: IntoIterator<Item=(K, V)>>(iter: T) -> SynonymMap<K, V> {
        let mut map = SynonymMap::new();
        for (k, v) in iter {
            map.insert(k, v);
        }
        map
    }
}

impl<K: Eq + Hash + Debug, V: Debug> Debug for SynonymMap<K, V> {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        self.vals.fmt(f)?;
        write!(f, " (synomyns: {:?})", self.syns)
    }
}
