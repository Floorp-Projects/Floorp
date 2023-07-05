use {crate::unreachable_unchecked, alloc::vec::Vec, core::mem::replace};

#[derive(Debug)]
enum Entry<T> {
    Vacant(usize),
    Occupied(T),
}
#[derive(Debug)]
pub(crate) struct Slab<T> {
    next_vacant: usize,
    entries: Vec<Entry<T>>,
}

impl<T> Slab<T> {
    pub fn new() -> Self {
        Slab {
            next_vacant: !0,
            entries: Vec::new(),
        }
    }

    /// Inserts value into this linked vec and returns index
    /// at which value can be accessed in constant time.
    pub fn insert(&mut self, value: T) -> usize {
        if self.next_vacant >= self.entries.len() {
            self.entries.push(Entry::Occupied(value));
            self.entries.len() - 1
        } else {
            match *unsafe { self.entries.get_unchecked(self.next_vacant) } {
                Entry::Vacant(next_vacant) => {
                    unsafe {
                        *self.entries.get_unchecked_mut(self.next_vacant) = Entry::Occupied(value);
                    }
                    replace(&mut self.next_vacant, next_vacant)
                }
                _ => unsafe { unreachable_unchecked() },
            }
        }
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub unsafe fn get_unchecked(&self, index: usize) -> &T {
        debug_assert!(index < self.len());

        match self.entries.get_unchecked(index) {
            Entry::Occupied(value) => value,
            _ => unreachable_unchecked(),
        }
    }

    pub unsafe fn get_unchecked_mut(&mut self, index: usize) -> &mut T {
        debug_assert!(index < self.len());

        match self.entries.get_unchecked_mut(index) {
            Entry::Occupied(value) => value,
            _ => unreachable_unchecked(),
        }
    }

    pub fn get(&self, index: usize) -> &T {
        match self.entries.get(index) {
            Some(Entry::Occupied(value)) => value,
            _ => panic!("Invalid index"),
        }
    }

    pub fn get_mut(&mut self, index: usize) -> &mut T {
        match self.entries.get_mut(index) {
            Some(Entry::Occupied(value)) => value,
            _ => panic!("Invalid index"),
        }
    }

    pub unsafe fn remove_unchecked(&mut self, index: usize) -> T {
        let entry = replace(
            self.entries.get_unchecked_mut(index),
            Entry::Vacant(self.next_vacant),
        );

        self.next_vacant = index;

        match entry {
            Entry::Occupied(value) => value,
            _ => unreachable_unchecked(),
        }
    }

    pub fn remove(&mut self, index: usize) -> T {
        match self.entries.get_mut(index) {
            Some(Entry::Occupied(_)) => unsafe { self.remove_unchecked(index) },
            _ => panic!("Invalid index"),
        }
    }
}
