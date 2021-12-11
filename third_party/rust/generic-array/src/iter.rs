//! `GenericArray` iterator implementation.

use super::{ArrayLength, GenericArray};
use core::{cmp, ptr, fmt, mem};
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

#[cfg(test)]
mod test {
    use super::*;

    fn send<I: Send>(_iter: I) {}

    #[test]
    fn test_send_iter() {
        send(GenericArray::from([1, 2, 3, 4]).into_iter());
    }
}

impl<T, N> GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    /// Returns the remaining items of this iterator as a slice
    #[inline]
    pub fn as_slice(&self) -> &[T] {
        &self.array.as_slice()[self.index..self.index_back]
    }

    /// Returns the remaining items of this iterator as a mutable slice
    #[inline]
    pub fn as_mut_slice(&mut self) -> &mut [T] {
        &mut self.array.as_mut_slice()[self.index..self.index_back]
    }
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

// Based on work in rust-lang/rust#49000
impl<T: fmt::Debug, N> fmt::Debug for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_tuple("GenericArrayIter")
            .field(&self.as_slice())
            .finish()
    }
}

impl<T, N> Drop for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    #[inline]
    fn drop(&mut self) {
        // Drop values that are still alive.
        for p in self.as_mut_slice() {
            unsafe {
                ptr::drop_in_place(p);
            }
        }
    }
}

// Based on work in rust-lang/rust#49000
impl<T: Clone, N> Clone for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    fn clone(&self) -> Self {
        // This places all cloned elements at the start of the new array iterator,
        // not at their original indices.
        unsafe {
            let mut iter = GenericArrayIter {
                array: ManuallyDrop::new(mem::uninitialized()),
                index: 0,
                index_back: 0,
            };

            for (dst, src) in iter.array.iter_mut().zip(self.as_slice()) {
                ptr::write(dst, src.clone());

                iter.index_back += 1;
            }

            iter
        }
    }
}

impl<T, N> Iterator for GenericArrayIter<T, N>
where
    N: ArrayLength<T>,
{
    type Item = T;

    #[inline]
    fn next(&mut self) -> Option<T> {
        if self.index < self.index_back {
            let p = unsafe { Some(ptr::read(self.array.get_unchecked(self.index))) };

            self.index += 1;

            p
        } else {
            None
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }

    #[inline]
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
        if self.index < self.index_back {
            self.index_back -= 1;

            unsafe { Some(ptr::read(self.array.get_unchecked(self.index_back))) }
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

// TODO: Implement `FusedIterator` and `TrustedLen` when stabilized