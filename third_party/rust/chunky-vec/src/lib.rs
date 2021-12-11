//! This crate provides a pin-safe, append-only vector which guarantees never
//! to move the storage for an element once it has been allocated.

use std::{
    ops::{Index, IndexMut},
    slice,
};

struct Chunk<T> {
    /// The elements of this chunk.
    elements: Vec<T>,
}

impl<T> Chunk<T> {
    fn with_capacity(capacity: usize) -> Self {
        let elements = Vec::with_capacity(capacity);
        assert_eq!(elements.capacity(), capacity);
        Self { elements }
    }

    fn len(&self) -> usize {
        self.elements.len()
    }

    /// Returns the number of available empty elements.
    fn available(&self) -> usize {
        self.elements.capacity() - self.elements.len()
    }

    /// Returns a shared reference to the element at the given index.
    ///
    /// # Panics
    ///
    /// Panics if the index is out of bounds.
    pub fn get(&self, index: usize) -> Option<&T> {
        self.elements.get(index)
    }

    /// Returns an exclusive reference to the element at the given index.
    ///
    /// # Panics
    ///
    /// Panics if the index is out of bounds.
    pub fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        self.elements.get_mut(index)
    }

    /// Pushes a new value into the fixed capacity entry.
    ///
    /// # Panics
    ///
    /// If the entry is already at its capacity.
    /// Note that this panic should never happen since the entry is only ever
    /// accessed by its outer chunk vector that checks before pushing.
    pub fn push(&mut self, new_value: T) {
        if self.available() == 0 {
            panic!("No available elements.")
        }
        self.elements.push(new_value);
    }

    pub fn push_get(&mut self, new_value: T) -> &mut T {
        self.push(new_value);
        unsafe {
            let last = self.elements.len() - 1;
            self.elements.get_unchecked_mut(last)
        }
    }

    /// Returns an iterator over the elements of the chunk.
    pub fn iter(&self) -> slice::Iter<T> {
        self.elements.iter()
    }

    /// Returns an iterator over the elements of the chunk.
    pub fn iter_mut(&mut self) -> slice::IterMut<T> {
        self.elements.iter_mut()
    }
}

impl<T> Index<usize> for Chunk<T> {
    type Output = T;

    fn index(&self, index: usize) -> &Self::Output {
        self.get(index).expect("index out of bounds")
    }
}

impl<T> IndexMut<usize> for Chunk<T> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        self.get_mut(index).expect("index out of bounds")
    }
}

/// Pin safe vector
///
/// An append only vector that never moves the backing store for each element.
pub struct ChunkyVec<T> {
    /// The chunks holding elements
    chunks: Vec<Chunk<T>>,
}

impl<T> Default for ChunkyVec<T> {
    fn default() -> Self {
        Self {
            chunks: Vec::default(),
        }
    }
}

impl<T> Unpin for ChunkyVec<T> {}

impl<T> ChunkyVec<T> {
    const DEFAULT_CAPACITY: usize = 32;

    pub fn len(&self) -> usize {
        if self.chunks.is_empty() {
            0
        } else {
            // # Safety - There is at least one chunk here.
            (self.chunks.len() - 1) * Self::DEFAULT_CAPACITY + self.chunks.last().unwrap().len()
        }
    }

    pub fn is_empty(&self) -> bool {
        // # Safety - Since it's impossible to pop, at least one chunk means we're not empty.
        self.chunks.is_empty()
    }

    /// Returns an iterator that yields shared references to the elements of the bucket vector.
    pub fn iter(&self) -> Iter<T> {
        Iter::new(self)
    }

    /// Returns an iterator that yields exclusive reference to the elements of the bucket vector.
    pub fn iter_mut(&mut self) -> IterMut<T> {
        IterMut::new(self)
    }

    /// Returns a shared reference to the element at the given index if any.
    pub fn get(&self, index: usize) -> Option<&T> {
        let (x, y) = (
            index / Self::DEFAULT_CAPACITY,
            index % Self::DEFAULT_CAPACITY,
        );
        self.chunks.get(x).and_then(|chunk| chunk.get(y))
    }

    /// Returns an exclusive reference to the element at the given index if any.
    pub fn get_mut(&mut self, index: usize) -> Option<&mut T> {
        let (x, y) = (
            index / Self::DEFAULT_CAPACITY,
            index % Self::DEFAULT_CAPACITY,
        );
        self.chunks.get_mut(x).and_then(|chunk| chunk.get_mut(y))
    }

    /// Pushes a new element onto the bucket vector.
    ///
    /// # Note
    ///
    /// This operation will never move other elements, reallocates or otherwise
    /// invalidate pointers of elements contained by the bucket vector.
    pub fn push(&mut self, new_value: T) {
        self.push_get(new_value);
    }

    pub fn push_get(&mut self, new_value: T) -> &mut T {
        if self.chunks.last().map(Chunk::available).unwrap_or_default() == 0 {
            self.chunks.push(Chunk::with_capacity(Self::DEFAULT_CAPACITY));
        }
        // Safety: Guaranteed to have a chunk with available elements
        unsafe {
            let last = self.chunks.len() - 1;
            self.chunks.get_unchecked_mut(last).push_get(new_value)
        }
    }
}

impl<T> Index<usize> for ChunkyVec<T> {
    type Output = T;

    fn index(&self, index: usize) -> &Self::Output {
        self.get(index).expect("index out of bounds")
    }
}

impl<T> IndexMut<usize> for ChunkyVec<T> {
    fn index_mut(&mut self, index: usize) -> &mut Self::Output {
        self.get_mut(index).expect("index out of bounds")
    }
}

/// An iterator yielding shared references to the elements of a ChunkyVec.
#[derive(Clone)]
pub struct Iter<'a, T> {
    /// Chunks iterator.
    chunks: slice::Iter<'a, Chunk<T>>,
    /// Forward iterator for `next`.
    iter: Option<slice::Iter<'a, T>>,
    /// Number of elements that are to be yielded by the iterator.
    len: usize,
}

impl<'a, T> Iter<'a, T> {
    /// Creates a new iterator over the ChunkyVec
    pub(crate) fn new(vec: &'a ChunkyVec<T>) -> Self {
        let len = vec.len();
        Self {
            chunks: vec.chunks.iter(),
            iter: None,
            len,
        }
    }
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(ref mut iter) = self.iter {
                if let front @ Some(_) = iter.next() {
                    self.len -= 1;
                    return front;
                }
            }
            self.iter = Some(self.chunks.next()?.iter());
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

impl<'a, T> ExactSizeIterator for Iter<'a, T> {
    fn len(&self) -> usize {
        self.len
    }
}

/// An iterator yielding exclusive references to the elements of a ChunkVec.
pub struct IterMut<'a, T> {
    /// Chunks iterator.
    chunks: slice::IterMut<'a, Chunk<T>>,
    /// Forward iterator for `next`.
    iter: Option<slice::IterMut<'a, T>>,
    /// Number of elements that are to be yielded by the iterator.
    len: usize,
}

impl<'a, T> IterMut<'a, T> {
    /// Creates a new iterator over the bucket vector.
    pub(crate) fn new(vec: &'a mut ChunkyVec<T>) -> Self {
        let len = vec.len();
        Self {
            chunks: vec.chunks.iter_mut(),
            iter: None,
            len,
        }
    }
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = &'a mut T;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(ref mut iter) = self.iter {
                if let front @ Some(_) = iter.next() {
                    self.len -= 1;
                    return front;
                }
            }
            self.iter = Some(self.chunks.next()?.iter_mut());
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

impl<'a, T> ExactSizeIterator for IterMut<'a, T> {
    fn len(&self) -> usize {
        self.len
    }
}
impl<'a, T> IntoIterator for &'a ChunkyVec<T> {
    type Item = &'a T;
    type IntoIter = Iter<'a, T>;

    fn into_iter(self) -> Iter<'a, T> {
        self.iter()
    }
}

impl<'a, T> IntoIterator for &'a mut ChunkyVec<T> {
    type Item = &'a mut T;
    type IntoIter = IterMut<'a, T>;

    fn into_iter(self) -> IterMut<'a, T> {
        self.iter_mut()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn iterate_empty() {
        let v = ChunkyVec::<usize>::default();
        for i in &v {
            println!("{:?}", i);
        }
    }

    #[test]
    fn iterate_multiple_chunks() {
        let mut v = ChunkyVec::<usize>::default();
        for i in 0..33 {
            v.push(i);
        }
        let mut iter = v.iter();
        for _ in 0..32 {
            iter.next();
        }
        assert_eq!(iter.next(), Some(&32));
        assert_eq!(iter.next(), None);
    }

    #[test]
    fn index_multiple_chunks() {
        let mut v = ChunkyVec::<usize>::default();
        for i in 0..33 {
            v.push(i);
        }
        assert_eq!(v.get(32), Some(&32));
        assert_eq!(v[32], 32);
    }
}
