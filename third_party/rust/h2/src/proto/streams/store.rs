use super::*;

use slab;

use indexmap::{self, IndexMap};

use std::fmt;
use std::marker::PhantomData;
use std::ops;

/// Storage for streams
#[derive(Debug)]
pub(super) struct Store {
    slab: slab::Slab<(StoreId, Stream)>,
    ids: IndexMap<StreamId, (usize, StoreId)>,
    counter: StoreId,
}

/// "Pointer" to an entry in the store
pub(super) struct Ptr<'a> {
    key: Key,
    store: &'a mut Store,
}

/// References an entry in the store.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct Key {
    index: usize,
    store_id: StoreId,
}

type StoreId = usize;

#[derive(Debug)]
pub(super) struct Queue<N> {
    indices: Option<store::Indices>,
    _p: PhantomData<N>,
}

pub(super) trait Next {
    fn next(stream: &Stream) -> Option<Key>;

    fn set_next(stream: &mut Stream, key: Option<Key>);

    fn take_next(stream: &mut Stream) -> Option<Key>;

    fn is_queued(stream: &Stream) -> bool;

    fn set_queued(stream: &mut Stream, val: bool);
}

/// A linked list
#[derive(Debug, Clone, Copy)]
struct Indices {
    pub head: Key,
    pub tail: Key,
}

pub(super) enum Entry<'a> {
    Occupied(OccupiedEntry<'a>),
    Vacant(VacantEntry<'a>),
}

pub(super) struct OccupiedEntry<'a> {
    ids: indexmap::map::OccupiedEntry<'a, StreamId, (usize, StoreId)>,
}

pub(super) struct VacantEntry<'a> {
    ids: indexmap::map::VacantEntry<'a, StreamId, (usize, StoreId)>,
    slab: &'a mut slab::Slab<(StoreId, Stream)>,
    counter: &'a mut usize,
}

pub(super) trait Resolve {
    fn resolve(&mut self, key: Key) -> Ptr;
}

// ===== impl Store =====

impl Store {
    pub fn new() -> Self {
        Store {
            slab: slab::Slab::new(),
            ids: IndexMap::new(),
            counter: 0,
        }
    }

    pub fn find_mut(&mut self, id: &StreamId) -> Option<Ptr> {
        let key = match self.ids.get(id) {
            Some(key) => *key,
            None => return None,
        };

        Some(Ptr {
            key: Key {
                index: key.0,
                store_id: key.1,
            },
            store: self,
        })
    }

    pub fn insert(&mut self, id: StreamId, val: Stream) -> Ptr {
        let store_id = self.counter;
        self.counter = self.counter.wrapping_add(1);
        let key = self.slab.insert((store_id, val));
        assert!(self.ids.insert(id, (key, store_id)).is_none());

        Ptr {
            key: Key {
                index: key,
                store_id,
            },
            store: self,
        }
    }

    pub fn find_entry(&mut self, id: StreamId) -> Entry {
        use self::indexmap::map::Entry::*;

        match self.ids.entry(id) {
            Occupied(e) => Entry::Occupied(OccupiedEntry {
                ids: e,
            }),
            Vacant(e) => Entry::Vacant(VacantEntry {
                ids: e,
                slab: &mut self.slab,
                counter: &mut self.counter,
            }),
        }
    }

    pub fn for_each<F, E>(&mut self, mut f: F) -> Result<(), E>
    where
        F: FnMut(Ptr) -> Result<(), E>,
    {
        let mut len = self.ids.len();
        let mut i = 0;

        while i < len {
            // Get the key by index, this makes the borrow checker happy
            let key = *self.ids.get_index(i).unwrap().1;

            f(Ptr {
                key: Key {
                    index: key.0,
                    store_id: key.1,
                },
                store: self,
            })?;

            // TODO: This logic probably could be better...
            let new_len = self.ids.len();

            if new_len < len {
                debug_assert!(new_len == len - 1);
                len -= 1;
            } else {
                i += 1;
            }
        }

        Ok(())
    }
}

impl Resolve for Store {
    fn resolve(&mut self, key: Key) -> Ptr {
        Ptr {
            key: key,
            store: self,
        }
    }
}

impl ops::Index<Key> for Store {
    type Output = Stream;

    fn index(&self, key: Key) -> &Self::Output {
        let slot = self.slab.index(key.index);
        assert_eq!(slot.0, key.store_id);
        &slot.1
    }
}

impl ops::IndexMut<Key> for Store {
    fn index_mut(&mut self, key: Key) -> &mut Self::Output {
        let slot = self.slab.index_mut(key.index);
        assert_eq!(slot.0, key.store_id);
        &mut slot.1
    }
}

impl Store {
    pub fn num_active_streams(&self) -> usize {
        self.ids.len()
    }

    #[cfg(feature = "unstable")]
    pub fn num_wired_streams(&self) -> usize {
        self.slab.len()
    }
}

impl Drop for Store {
    fn drop(&mut self) {
        use std::thread;

        if !thread::panicking() {
            debug_assert!(self.slab.is_empty());
        }
    }
}

// ===== impl Queue =====

impl<N> Queue<N>
where
    N: Next,
{
    pub fn new() -> Self {
        Queue {
            indices: None,
            _p: PhantomData,
        }
    }

    pub fn take(&mut self) -> Self {
        Queue {
            indices: self.indices.take(),
            _p: PhantomData,
        }
    }

    /// Queue the stream.
    ///
    /// If the stream is already contained by the list, return `false`.
    pub fn push(&mut self, stream: &mut store::Ptr) -> bool {
        trace!("Queue::push");

        if N::is_queued(stream) {
            trace!(" -> already queued");
            return false;
        }

        N::set_queued(stream, true);

        // The next pointer shouldn't be set
        debug_assert!(N::next(stream).is_none());

        // Queue the stream
        match self.indices {
            Some(ref mut idxs) => {
                trace!(" -> existing entries");

                // Update the current tail node to point to `stream`
                let key = stream.key();
                N::set_next(&mut stream.resolve(idxs.tail), Some(key));

                // Update the tail pointer
                idxs.tail = stream.key();
            },
            None => {
                trace!(" -> first entry");
                self.indices = Some(store::Indices {
                    head: stream.key(),
                    tail: stream.key(),
                });
            },
        }

        true
    }

    pub fn pop<'a, R>(&mut self, store: &'a mut R) -> Option<store::Ptr<'a>>
    where
        R: Resolve,
    {
        if let Some(mut idxs) = self.indices {
            let mut stream = store.resolve(idxs.head);

            if idxs.head == idxs.tail {
                assert!(N::next(&*stream).is_none());
                self.indices = None;
            } else {
                idxs.head = N::take_next(&mut *stream).unwrap();
                self.indices = Some(idxs);
            }

            debug_assert!(N::is_queued(&*stream));
            N::set_queued(&mut *stream, false);

            return Some(stream);
        }

        None
    }

    pub fn pop_if<'a, R, F>(&mut self, store: &'a mut R, f: F) -> Option<store::Ptr<'a>>
    where
        R: Resolve,
        F: Fn(&Stream) -> bool,
    {
        if let Some(idxs) = self.indices {
            let should_pop = f(&store.resolve(idxs.head));
            if should_pop {
                return self.pop(store);
            }
        }

        None
    }
}

// ===== impl Ptr =====

impl<'a> Ptr<'a> {
    /// Returns the Key associated with the stream
    pub fn key(&self) -> Key {
        self.key
    }

    pub fn store_mut(&mut self) -> &mut Store {
        &mut self.store
    }

    /// Remove the stream from the store
    pub fn remove(self) -> StreamId {
        // The stream must have been unlinked before this point
        debug_assert!(!self.store.ids.contains_key(&self.id));

        // Remove the stream state
        self.store.slab.remove(self.key.index).1.id
    }

    /// Remove the StreamId -> stream state association.
    ///
    /// This will effectively remove the stream as far as the H2 protocol is
    /// concerned.
    pub fn unlink(&mut self) {
        let id = self.id;
        self.store.ids.remove(&id);
    }
}

impl<'a> Resolve for Ptr<'a> {
    fn resolve(&mut self, key: Key) -> Ptr {
        Ptr {
            key: key,
            store: &mut *self.store,
        }
    }
}

impl<'a> ops::Deref for Ptr<'a> {
    type Target = Stream;

    fn deref(&self) -> &Stream {
        &self.store.slab[self.key.index].1
    }
}

impl<'a> ops::DerefMut for Ptr<'a> {
    fn deref_mut(&mut self) -> &mut Stream {
        &mut self.store.slab[self.key.index].1
    }
}

impl<'a> fmt::Debug for Ptr<'a> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        (**self).fmt(fmt)
    }
}

// ===== impl OccupiedEntry =====

impl<'a> OccupiedEntry<'a> {
    pub fn key(&self) -> Key {
        let tup = self.ids.get();
        Key {
            index: tup.0,
            store_id: tup.1,
        }
    }
}

// ===== impl VacantEntry =====

impl<'a> VacantEntry<'a> {
    pub fn insert(self, value: Stream) -> Key {
        // Insert the value in the slab
        let store_id = *self.counter;
        *self.counter = store_id.wrapping_add(1);
        let index = self.slab.insert((store_id, value));

        // Insert the handle in the ID map
        self.ids.insert((index, store_id));

        Key {
            index,
            store_id,
        }
    }
}
