use std::{fmt, mem, usize};
use std::iter::IntoIterator;
use std::ops;
use std::marker::PhantomData;

/// A preallocated chunk of memory for storing objects of the same type.
pub struct Slab<T, I = usize> {
    // Chunk of memory
    entries: Vec<Slot<T>>,

    // Number of Filled elements currently in the slab
    len: usize,

    // Offset of the next available slot in the slab. Set to the slab's
    // capacity when the slab is full.
    next: usize,

    _marker: PhantomData<I>,
}

/// A handle to an occupied slot in the `Slab`
pub struct Entry<'a, T: 'a, I: 'a> {
    slab: &'a mut Slab<T, I>,
    idx: usize,
}

/// A handle to a vacant slot in the `Slab`
pub struct VacantEntry<'a, T: 'a, I: 'a> {
    slab: &'a mut Slab<T, I>,
    idx: usize,
}

/// An iterator over the values stored in the `Slab`
pub struct Iter<'a, T: 'a, I: 'a> {
    slab: &'a Slab<T, I>,
    cur_idx: usize,
    yielded: usize,
}

/// A mutable iterator over the values stored in the `Slab`
pub struct IterMut<'a, T: 'a, I: 'a> {
    slab: *mut Slab<T, I>,
    cur_idx: usize,
    yielded: usize,
    _marker: PhantomData<&'a mut ()>,
}

enum Slot<T> {
    Empty(usize),
    Filled(T),
    Invalid,
}

unsafe impl<T, I> Send for Slab<T, I> where T: Send {}

macro_rules! some {
    ($expr:expr) => (match $expr {
        Some(val) => val,
        None => return None,
    })
}

impl<T, I> Slab<T, I> {
    /// Returns an empty `Slab` with the requested capacity
    pub fn with_capacity(capacity: usize) -> Slab<T, I> {
        let entries = (1..capacity + 1)
            .map(Slot::Empty)
            .collect::<Vec<_>>();

        Slab {
            entries: entries,
            next: 0,
            len: 0,
            _marker: PhantomData,
        }
    }

    /// Returns the number of values stored by the `Slab`
    pub fn len(&self) -> usize {
        self.len
    }

    /// Returns the total capacity of the `Slab`
    pub fn capacity(&self) -> usize {
        self.entries.len()
    }

    /// Returns true if the `Slab` is storing no values
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Returns the number of available slots remaining in the `Slab`
    pub fn available(&self) -> usize {
        self.entries.len() - self.len
    }

    /// Returns true if the `Slab` has available slots
    pub fn has_available(&self) -> bool {
        self.available() > 0
    }
}

impl<T, I: Into<usize> + From<usize>> Slab<T, I> {
    /// Returns true if the `Slab` contains a value for the given token
    pub fn contains(&self, idx: I) -> bool {
        self.get(idx).is_some()
    }

    /// Get a reference to the value associated with the given token
    pub fn get(&self, idx: I) -> Option<&T> {
        let idx = some!(self.local_index(idx));

        match self.entries[idx] {
            Slot::Filled(ref val) => Some(val),
            Slot::Empty(_) => None,
            Slot::Invalid => panic!("Slab corrupt"),
        }
    }

    /// Get a mutable reference to the value associated with the given token
    pub fn get_mut(&mut self, idx: I) -> Option<&mut T> {
        let idx = some!(self.local_index(idx));

        match self.entries[idx] {
            Slot::Filled(ref mut v) => Some(v),
            _ => None,
        }
    }

    /// Insert a value into the slab, returning the associated token
    pub fn insert(&mut self, val: T) -> Result<I, T> {
        match self.vacant_entry() {
            Some(entry) => Ok(entry.insert(val).index()),
            None => Err(val),
        }
    }

    /// Returns a handle to an entry.
    ///
    /// This allows more advanced manipulation of the value stored at the given
    /// index.
    pub fn entry(&mut self, idx: I) -> Option<Entry<T, I>> {
        let idx = some!(self.local_index(idx));

        match self.entries[idx] {
            Slot::Filled(_) => {
                Some(Entry {
                    slab: self,
                    idx: idx,
                })
            }
            Slot::Empty(_) => None,
            Slot::Invalid => panic!("Slab corrupt"),
        }
    }

    /// Returns a handle to a vacant entry.
    ///
    /// This allows optionally inserting a value that is constructed with the
    /// index.
    pub fn vacant_entry(&mut self) -> Option<VacantEntry<T, I>> {
        let idx = self.next;

        if idx >= self.entries.len() {
            return None;
        }

        Some(VacantEntry {
            slab: self,
            idx: idx,
        })
    }

    /// Releases the given slot
    pub fn remove(&mut self, idx: I) -> Option<T> {
        self.entry(idx).map(Entry::remove)
    }

    /// Retain only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&e)` returns false.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    pub fn retain<F>(&mut self, mut f: F)
        where F: FnMut(&T) -> bool
    {
        for i in 0..self.entries.len() {
            if let Some(e) = self.entry(I::from(i)) {
                if !f(e.get()) {
                    e.remove();
                }
            }
        }
    }

    /// An iterator for visiting all elements stored in the `Slab`
    pub fn iter(&self) -> Iter<T, I> {
        Iter {
            slab: self,
            cur_idx: 0,
            yielded: 0,
        }
    }

    /// A mutable iterator for visiting all elements stored in the `Slab`
    pub fn iter_mut(&mut self) -> IterMut<T, I> {
        IterMut {
            slab: self as *mut Slab<T, I>,
            cur_idx: 0,
            yielded: 0,
            _marker: PhantomData,
        }
    }

    /// Empty the slab, by freeing all entries
    pub fn clear(&mut self) {
        for (i, e) in self.entries.iter_mut().enumerate() {
            *e = Slot::Empty(i + 1)
        }
        self.next = 0;
        self.len = 0;
    }

    /// Reserves the minimum capacity for exactly `additional` more elements to
    /// be inserted in the given `Slab`. Does nothing if the capacity is
    /// already sufficient.
    pub fn reserve_exact(&mut self, additional: usize) {
        let prev_len = self.entries.len();

        // Ensure `entries_num` isn't too big
        assert!(additional < usize::MAX - prev_len, "capacity too large");

        let prev_len_next = prev_len + 1;
        self.entries.extend((prev_len_next..(prev_len_next + additional)).map(Slot::Empty));

        debug_assert_eq!(self.entries.len(), prev_len + additional);
    }

    fn insert_at(&mut self, idx: usize, value: T) -> I {
        self.next = match self.entries[idx] {
            Slot::Empty(next) => next,
            Slot::Filled(_) => panic!("Index already contains value"),
            Slot::Invalid => panic!("Slab corrupt"),
        };

        self.entries[idx] = Slot::Filled(value);
        self.len += 1;

        I::from(idx)
    }

    fn replace(&mut self, idx: usize, e: Slot<T>) -> Option<T> {
        if let Slot::Filled(val) = mem::replace(&mut self.entries[idx], e) {
            self.next = idx;
            return Some(val);
        }

        None
    }

    fn local_index(&self, idx: I) -> Option<usize> {
        let idx: usize = idx.into();

        if idx >= self.entries.len() {
            return None;
        }

        Some(idx)
    }
}

impl<T, I: From<usize> + Into<usize>> ops::Index<I> for Slab<T, I> {
    type Output = T;

    fn index(&self, index: I) -> &T {
        self.get(index).expect("invalid index")
    }
}

impl<T, I: From<usize> + Into<usize>> ops::IndexMut<I> for Slab<T, I> {
    fn index_mut(&mut self, index: I) -> &mut T {
        self.get_mut(index).expect("invalid index")
    }
}

impl<T, I> fmt::Debug for Slab<T, I>
    where T: fmt::Debug,
          I: fmt::Debug,
{
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt,
               "Slab {{ len: {}, cap: {} }}",
               self.len,
               self.capacity())
    }
}

impl<'a, T, I: From<usize> + Into<usize>> IntoIterator for &'a Slab<T, I> {
    type Item = &'a T;
    type IntoIter = Iter<'a, T, I>;

    fn into_iter(self) -> Iter<'a, T, I> {
        self.iter()
    }
}

impl<'a, T, I: From<usize> + Into<usize>> IntoIterator for &'a mut Slab<T, I> {
    type Item = &'a mut T;
    type IntoIter = IterMut<'a, T, I>;

    fn into_iter(self) -> IterMut<'a, T, I> {
        self.iter_mut()
    }
}

/*
 *
 * ===== Entry =====
 *
 */

impl<'a, T, I: From<usize> + Into<usize>> Entry<'a, T, I> {

    /// Replace the value stored in the entry
    pub fn replace(&mut self, val: T) -> T {
        match mem::replace(&mut self.slab.entries[self.idx], Slot::Filled(val)) {
            Slot::Filled(v) => v,
            _ => panic!("Slab corrupt"),
        }
    }

    /// Apply the function to the current value, replacing it with the result
    /// of the function.
    pub fn replace_with<F>(&mut self, f: F)
        where F: FnOnce(T) -> T
    {
        let idx = self.idx;

        // Take the value out of the entry, temporarily setting it to Invalid
        let val = match mem::replace(&mut self.slab.entries[idx], Slot::Invalid) {
            Slot::Filled(v) => f(v),
            _ => panic!("Slab corrupt"),
        };

        self.slab.entries[idx] = Slot::Filled(val);
    }

    /// Remove and return the value stored in the entry
    pub fn remove(self) -> T {
        let next = self.slab.next;

        if let Some(v) = self.slab.replace(self.idx, Slot::Empty(next)) {
            self.slab.len -= 1;
            v
        } else {
            panic!("Slab corrupt");
        }
    }

    /// Get a reference to the value stored in the entry
    pub fn get(&self) -> &T {
        let idx = self.index();
        self.slab
            .get(idx)
            .expect("Filled slot in Entry")
    }

    /// Get a mutable reference to the value stored in the entry
    pub fn get_mut(&mut self) -> &mut T {
        let idx = self.index();
        self.slab
            .get_mut(idx)
            .expect("Filled slot in Entry")
    }

    /// Convert the entry handle to a mutable reference
    pub fn into_mut(self) -> &'a mut T {
        let idx = self.index();
        self.slab
            .get_mut(idx)
            .expect("Filled slot in Entry")
    }

    /// Return the entry index
    pub fn index(&self) -> I {
        I::from(self.idx)
    }
}

/*
 *
 * ===== VacantEntry =====
 *
 */

impl<'a, T, I: From<usize> + Into<usize>> VacantEntry<'a, T, I> {
    /// Insert a value into the entry
    pub fn insert(self, val: T) -> Entry<'a, T, I> {
        self.slab.insert_at(self.idx, val);

        Entry {
            slab: self.slab,
            idx: self.idx,
        }
    }

    /// Returns the entry index
    pub fn index(&self) -> I {
        I::from(self.idx)
    }
}

/*
 *
 * ===== Iter =====
 *
 */

impl<'a, T, I> Iterator for Iter<'a, T, I> {
    type Item = &'a T;

    fn next(&mut self) -> Option<&'a T> {
        while self.yielded < self.slab.len {
            match self.slab.entries[self.cur_idx] {
                Slot::Filled(ref v) => {
                    self.cur_idx += 1;
                    self.yielded += 1;
                    return Some(v);
                }
                Slot::Empty(_) => {
                    self.cur_idx += 1;
                }
                Slot::Invalid => {
                    panic!("Slab corrupt");
                }
            }
        }

        None
    }
}

/*
 *
 * ===== IterMut =====
 *
 */

impl<'a, T, I> Iterator for IterMut<'a, T, I> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<&'a mut T> {
        unsafe {
            while self.yielded < (*self.slab).len {
                let idx = self.cur_idx;

                match (*self.slab).entries[idx] {
                    Slot::Filled(ref mut v) => {
                        self.cur_idx += 1;
                        self.yielded += 1;
                        return Some(v);
                    }
                    Slot::Empty(_) => {
                        self.cur_idx += 1;
                    }
                    Slot::Invalid => {
                        panic!("Slab corrupt");
                    }
                }
            }

            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Copy, Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
    pub struct MyIndex(pub usize);

    impl From<usize> for MyIndex {
        fn from(i: usize) -> MyIndex {
            MyIndex(i)
        }
    }

    impl Into<usize> for MyIndex {
        fn into(self) -> usize {
            self.0
        }
    }

    #[test]
    fn test_index_trait() {
        let mut slab = Slab::<usize, MyIndex>::with_capacity(1);
        let idx = slab.insert(10).ok().expect("Failed to insert");
        assert_eq!(idx, MyIndex(0));
        assert_eq!(slab[idx], 10);
    }

    #[test]
    fn test_insertion() {
        let mut slab = Slab::<usize, usize>::with_capacity(1);
        assert_eq!(slab.is_empty(), true);
        assert_eq!(slab.has_available(), true);
        assert_eq!(slab.available(), 1);
        let idx = slab.insert(10).ok().expect("Failed to insert");
        assert_eq!(slab[idx], 10);
        assert_eq!(slab.is_empty(), false);
        assert_eq!(slab.has_available(), false);
        assert_eq!(slab.available(), 0);
    }

    #[test]
    fn test_insert_with() {
        let mut slab = Slab::<usize, usize>::with_capacity(1);

        {
            let e = slab.vacant_entry().unwrap();
            assert_eq!(e.index(), 0);
            let e = e.insert(5);
            assert_eq!(5, *e.get());
        }

        assert_eq!(Some(&5), slab.get(0));
    }

    #[test]
    fn test_repeated_insertion() {
        let mut slab = Slab::<usize, usize>::with_capacity(10);

        for i in 0..10 {
            let idx = slab.insert(i + 10).ok().expect("Failed to insert");
            assert_eq!(slab[idx], i + 10);
        }

        slab.insert(20).err().expect("Inserted when full");
    }

    #[test]
    fn test_repeated_insertion_and_removal() {
        let mut slab = Slab::<usize, usize>::with_capacity(10);
        let mut indices = vec![];

        for i in 0..10 {
            let idx = slab.insert(i + 10).ok().expect("Failed to insert");
            indices.push(idx);
            assert_eq!(slab[idx], i + 10);
        }

        for &i in indices.iter() {
            slab.remove(i);
        }

        slab.insert(20).ok().expect("Failed to insert in newly empty slab");
    }

    #[test]
    fn test_insertion_when_full() {
        let mut slab = Slab::<usize, usize>::with_capacity(1);
        slab.insert(10).ok().expect("Failed to insert");
        slab.insert(10).err().expect("Inserted into a full slab");
    }

    #[test]
    fn test_removal_at_boundries() {
        let mut slab = Slab::<usize, usize>::with_capacity(1);
        assert_eq!(slab.remove(0), None);
        assert_eq!(slab.remove(1), None);
    }

    #[test]
    fn test_removal_is_successful() {
        let mut slab = Slab::<usize, usize>::with_capacity(1);
        let t1 = slab.insert(10).ok().expect("Failed to insert");
        slab.remove(t1);
        let t2 = slab.insert(20).ok().expect("Failed to insert");
        assert_eq!(slab[t2], 20);
    }

    #[test]
    fn test_remove_empty_entry() {
        let mut s = Slab::<(), usize>::with_capacity(3);
        let t1 = s.insert(()).unwrap();
        assert!(s.remove(t1).is_some());
        assert!(s.remove(t1).is_none());
        assert!(s.insert(()).is_ok());
        assert!(s.insert(()).is_ok());
    }

    #[test]
    fn test_mut_retrieval() {
        let mut slab = Slab::<_, usize>::with_capacity(1);
        let t1 = slab.insert("foo".to_string()).ok().expect("Failed to insert");

        slab[t1].push_str("bar");

        assert_eq!(&slab[t1][..], "foobar");
    }

    #[test]
    #[should_panic]
    fn test_reusing_slots_1() {
        let mut slab = Slab::<usize, usize>::with_capacity(16);

        let t0 = slab.insert(123).unwrap();
        let t1 = slab.insert(456).unwrap();

        assert!(slab.len() == 2);
        assert!(slab.available() == 14);

        slab.remove(t0);

        assert!(slab.len() == 1, "actual={}", slab.len());
        assert!(slab.available() == 15);

        slab.remove(t1);

        assert!(slab.len() == 0);
        assert!(slab.available() == 16);

        let _ = slab[t1];
    }

    #[test]
    fn test_reusing_slots_2() {
        let mut slab = Slab::<usize, usize>::with_capacity(16);

        let t0 = slab.insert(123).unwrap();

        assert!(slab[t0] == 123);
        assert!(slab.remove(t0) == Some(123));

        let t0 = slab.insert(456).unwrap();

        assert!(slab[t0] == 456);

        let t1 = slab.insert(789).unwrap();

        assert!(slab[t0] == 456);
        assert!(slab[t1] == 789);

        assert!(slab.remove(t0).unwrap() == 456);
        assert!(slab.remove(t1).unwrap() == 789);

        assert!(slab.len() == 0);
    }

    #[test]
    #[should_panic]
    fn test_accessing_out_of_bounds() {
        let slab = Slab::<usize, usize>::with_capacity(16);
        slab[0];
    }

    #[test]
    #[should_panic]
    fn test_capacity_too_large1() {
        use std::usize;
        Slab::<usize, usize>::with_capacity(usize::MAX);
    }

    #[test]
    #[should_panic]
    fn test_capacity_too_large_in_reserve_exact() {
        use std::usize;
        let mut slab = Slab::<usize, usize>::with_capacity(100);
        slab.reserve_exact(usize::MAX - 100);
    }

    #[test]
    fn test_contains() {
        let mut slab = Slab::with_capacity(16);
        assert!(!slab.contains(0));

        let idx = slab.insert(111).unwrap();
        assert!(slab.contains(idx));
    }

    #[test]
    fn test_get() {
        let mut slab = Slab::<usize, usize>::with_capacity(16);
        let tok = slab.insert(5).unwrap();
        assert_eq!(slab.get(tok), Some(&5));
        assert_eq!(slab.get(1), None);
        assert_eq!(slab.get(23), None);
    }

    #[test]
    fn test_get_mut() {
        let mut slab = Slab::<u32, usize>::with_capacity(16);
        let tok = slab.insert(5u32).unwrap();
        {
            let mut_ref = slab.get_mut(tok).unwrap();
            assert_eq!(*mut_ref, 5);
            *mut_ref = 12;
        }
        assert_eq!(slab[tok], 12);
        assert_eq!(slab.get_mut(1), None);
        assert_eq!(slab.get_mut(23), None);
    }

    #[test]
    fn test_replace() {
        let mut slab = Slab::<usize, usize>::with_capacity(16);
        let tok = slab.insert(5).unwrap();

        slab.entry(tok).unwrap().replace(6);
        assert!(slab.entry(tok + 1).is_none());

        assert_eq!(slab[tok], 6);
        assert_eq!(slab.len(), 1);
    }

    #[test]
    fn test_replace_again() {
        let mut slab = Slab::<usize, usize>::with_capacity(16);
        let tok = slab.insert(5).unwrap();

        slab.entry(tok).unwrap().replace(6);
        slab.entry(tok).unwrap().replace(7);
        slab.entry(tok).unwrap().replace(8);
        assert_eq!(slab[tok], 8);
    }

    #[test]
    fn test_replace_with() {
        let mut slab = Slab::<u32, usize>::with_capacity(16);
        let tok = slab.insert(5u32).unwrap();
        slab.entry(tok).unwrap().replace_with(|x| x + 1);
        assert_eq!(slab[tok], 6);
    }

    #[test]
    fn test_retain() {
        let mut slab = Slab::<usize, usize>::with_capacity(2);
        let tok1 = slab.insert(0).unwrap();
        let tok2 = slab.insert(1).unwrap();
        slab.retain(|x| x % 2 == 0);
        assert_eq!(slab.len(), 1);
        assert_eq!(slab[tok1], 0);
        assert_eq!(slab.contains(tok2), false);
    }

    #[test]
    fn test_iter() {
        let mut slab = Slab::<u32, usize>::with_capacity(4);
        for i in 0..4 {
            slab.insert(i).unwrap();
        }

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![0, 1, 2, 3]);

        slab.remove(1);

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![0, 2, 3]);
    }

    #[test]
    fn test_iter_mut() {
        let mut slab = Slab::<u32, usize>::with_capacity(4);
        for i in 0..4 {
            slab.insert(i).unwrap();
        }
        for e in slab.iter_mut() {
            *e = *e + 1;
        }

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![1, 2, 3, 4]);

        slab.remove(2);
        for e in slab.iter_mut() {
            *e = *e + 1;
        }

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![2, 3, 5]);
    }

    #[test]
    fn test_reserve_exact() {
        let mut slab = Slab::<u32, usize>::with_capacity(4);
        for i in 0..4 {
            slab.insert(i).unwrap();
        }

        assert!(slab.insert(0).is_err());

        slab.reserve_exact(3);

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![0, 1, 2, 3]);

        for i in 0..3 {
            slab.insert(i).unwrap();
        }
        assert!(slab.insert(0).is_err());

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![0, 1, 2, 3, 0, 1, 2]);
    }

    #[test]
    fn test_clear() {
        let mut slab = Slab::<u32, usize>::with_capacity(4);
        for i in 0..4 {
            slab.insert(i).unwrap();
        }

        // clear full
        slab.clear();

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![]);

        for i in 0..2 {
            slab.insert(i).unwrap();
        }

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![0, 1]);


        // clear half-filled
        slab.clear();

        let vals: Vec<u32> = slab.iter().map(|r| *r).collect();
        assert_eq!(vals, vec![]);
    }
}
