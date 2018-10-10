//! `GenericArray` iterator implementation.

use super::{ArrayLength, GenericArray};
use core::{cmp, ptr};
use core::mem::ManuallyDrop;

/// An iterator that moves out of a `GenericArray`
pub struct GenericArrayIter<T, N: ArrayLength<T>> {
    // Invariants: index <= index_back <= N
    // Only values in array[index..index_back] are alive at any given time.
    // Values from array[..index] and array[index_back..] are already moved/dropped.
    array: ManuallyDrop<GenericArray<T, N>>,
    index: usize,
    index_back: usize,
}

impl<T, N> IntoIterator for GenericArray<T, N>
where
    N: ArrayLength<T>,
{
    type Item = T;
    type IntoIter = GenericArrayIter<T, N>;

    fn into_iter(self) -> Self::IntoIter {
        GenericArrayIter {
            array: ManuallyDrop::new(self),
            index: 0,
            index_back: N::to_usize(),
        }
    }
}

impl<T, N> Drop for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    fn drop(&mut self) {
        // Drop values that are still alive.
        for p in &mut self.array[self.index..self.index_back] {
            unsafe {
                ptr::drop_in_place(p);
            }
        }
    }
}

impl<T, N> Iterator for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    type Item = T;

    fn next(&mut self) -> Option<T> {
        if self.len() > 0 {
            unsafe {
                let p = self.array.get_unchecked(self.index);
                self.index += 1;
                Some(ptr::read(p))
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }

    fn count(self) -> usize {
        self.len()
    }

    fn nth(&mut self, n: usize) -> Option<T> {
        // First consume values prior to the nth.
        let ndrop = cmp::min(n, self.len());
        for p in &mut self.array[self.index..self.index + ndrop] {
            self.index += 1;
            unsafe {
                ptr::drop_in_place(p);
            }
        }

        self.next()
    }

    fn last(mut self) -> Option<T> {
        // Note, everything else will correctly drop first as `self` leaves scope.
        self.next_back()
    }
}

impl<T, N> DoubleEndedIterator for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    fn next_back(&mut self) -> Option<T> {
        if self.len() > 0 {
            self.index_back -= 1;
            unsafe {
                let p = self.array.get_unchecked(self.index_back);
                Some(ptr::read(p))
            }
        } else {
            None
        }
    }
}

impl<T, N> ExactSizeIterator for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    fn len(&self) -> usize {
        self.index_back - self.index
    }
}
