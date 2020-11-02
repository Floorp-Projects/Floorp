use lock_api::RawRwLock;
use std::{cell::UnsafeCell, collections::hash_map::HashMap, fmt, hash, ops};

/// `StorageMap` is a wrapper around `HashMap` that allows efficient concurrent
/// access for the use case when only rarely the missing elements need to be created.
pub struct StorageMap<L, M> {
    lock: L,
    map: UnsafeCell<M>,
}

unsafe impl<L: Send, M> Send for StorageMap<L, M> {}
unsafe impl<L: Sync, M> Sync for StorageMap<L, M> {}

impl<L: RawRwLock, M: Default> Default for StorageMap<L, M> {
    fn default() -> Self {
        StorageMap {
            lock: L::INIT,
            map: UnsafeCell::new(M::default()),
        }
    }
}

impl<L, M: fmt::Debug> fmt::Debug for StorageMap<L, M> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        self.map.get().fmt(formatter)
    }
}

/// An element guard that releases the lock when dropped.
pub struct StorageMapGuard<'a, L: 'a + RawRwLock, V: 'a> {
    lock: &'a L,
    value: &'a V,
    exclusive: bool,
}

impl<'a, L: RawRwLock, V> ops::Deref for StorageMapGuard<'a, L, V> {
    type Target = V;
    fn deref(&self) -> &V {
        self.value
    }
}

impl<'a, L: RawRwLock, V> Drop for StorageMapGuard<'a, L, V> {
    fn drop(&mut self) {
        unsafe {
            if self.exclusive {
                self.lock.unlock_exclusive();
            } else {
                self.lock.unlock_shared();
            }
        }
    }
}

pub struct WholeMapWriteGuard<'a, L: 'a + RawRwLock, M: 'a> {
    lock: &'a L,
    map: &'a mut M,
}

impl<'a, L: RawRwLock, M> ops::Deref for WholeMapWriteGuard<'a, L, M> {
    type Target = M;
    fn deref(&self) -> &M {
        self.map
    }
}

impl<'a, L: RawRwLock, M> ops::DerefMut for WholeMapWriteGuard<'a, L, M> {
    fn deref_mut(&mut self) -> &mut M {
        self.map
    }
}

impl<'a, L: RawRwLock, V> Drop for WholeMapWriteGuard<'a, L, V> {
    fn drop(&mut self) {
        unsafe {
            self.lock.unlock_exclusive();
        }
    }
}

/// Result of preparing a particular key.
pub enum PrepareResult {
    /// Nothing is created, the key/value pair is already there.
    AlreadyExists,
    /// Key was not found, but value creation failed.
    UnableToCreate,
    /// Key was not found, but now the value has been created and inserted.
    Created,
}

impl<L, K, V, S> StorageMap<L, HashMap<K, V, S>>
where
    L: RawRwLock,
    K: Clone + Eq + hash::Hash,
    S: hash::BuildHasher,
{
    /// Create a new storage map with the given hasher.
    pub fn with_hasher(hash_builder: S) -> Self {
        StorageMap {
            lock: L::INIT,
            map: UnsafeCell::new(HashMap::with_hasher(hash_builder)),
        }
    }

    /// Get a value associated with the key. The method assumes that more often then not
    /// the value is already there. If it's not - the closure will be called to create one.
    /// This closure is expected to always produce the same value given the same key.
    pub fn get_or_create_with<'a, F: FnOnce() -> V>(
        &'a self,
        key: &K,
        create_fn: F,
    ) -> StorageMapGuard<'a, L, V> {
        self.lock.lock_shared();
        // try mapping for reading first
        let map = unsafe { &*self.map.get() };
        if let Some(value) = map.get(key) {
            return StorageMapGuard {
                lock: &self.lock,
                value,
                exclusive: false,
            };
        }
        unsafe {
            self.lock.unlock_shared();
        }
        // now actually lock for writes
        let value = create_fn();
        self.lock.lock_exclusive();
        let map = unsafe { &mut *self.map.get() };
        StorageMapGuard {
            lock: &self.lock,
            value: &*map.entry(key.clone()).or_insert(value),
            exclusive: true,
        }
    }

    /// Make sure the given key is in the map, as a way to warm up
    /// future run-time access to the map at the initialization stage.
    pub fn prepare_maybe<F: FnOnce() -> Option<V>>(&self, key: &K, create_fn: F) -> PrepareResult {
        self.lock.lock_shared();
        // try mapping for reading first
        let map = unsafe { &*self.map.get() };
        let has = map.contains_key(key);
        unsafe {
            self.lock.unlock_shared();
        }
        if has {
            return PrepareResult::AlreadyExists;
        }
        // try creating a new value
        let value = match create_fn() {
            Some(value) => value,
            None => return PrepareResult::UnableToCreate,
        };
        // now actually lock for writes
        self.lock.lock_exclusive();
        let map = unsafe { &mut *self.map.get() };
        map.insert(key.clone(), value);
        unsafe {
            self.lock.unlock_exclusive();
        }
        PrepareResult::Created
    }

    /// Lock the whole map for writing.
    pub fn whole_write(&self) -> WholeMapWriteGuard<L, HashMap<K, V, S>> {
        self.lock.lock_exclusive();
        WholeMapWriteGuard {
            lock: &self.lock,
            map: unsafe { &mut *self.map.get() },
        }
    }
}
