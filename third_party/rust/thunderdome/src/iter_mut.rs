use std::convert::TryInto;
use std::iter::{Enumerate, ExactSizeIterator, FusedIterator};
use std::slice;

use crate::arena::{Entry, Index};

/// See [`Arena::iter_mut`](crate::Arena::iter_mut).
pub struct IterMut<'a, T> {
    pub(crate) len: u32,
    pub(crate) inner: Enumerate<slice::IterMut<'a, Entry<T>>>,
}

impl<'a, T> Iterator for IterMut<'a, T> {
    type Item = (Index, &'a mut T);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.len == 0 {
                return None;
            }

            match self.inner.next()? {
                (_, Entry::Empty(_)) => continue,
                (slot, Entry::Occupied(occupied)) => {
                    self.len = self
                        .len
                        .checked_sub(1)
                        .unwrap_or_else(|| unreachable!("Underflowed u32 trying to iterate Arena"));

                    let slot: u32 = slot
                        .try_into()
                        .unwrap_or_else(|_| unreachable!("Overflowed u32 trying to iterate Arena"));

                    let index = Index {
                        slot,
                        generation: occupied.generation,
                    };

                    return Some((index, &mut occupied.value));
                }
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len as usize, Some(self.len as usize))
    }
}

impl<'a, T> FusedIterator for IterMut<'a, T> {}
impl<'a, T> ExactSizeIterator for IterMut<'a, T> {}

#[cfg(test)]
mod test {
    use crate::Arena;

    use std::collections::HashSet;

    #[test]
    fn iter_mut() {
        let mut arena = Arena::with_capacity(2);
        let one = arena.insert(1);
        let two = arena.insert(2);

        let mut pairs = HashSet::new();
        let mut iter = arena.iter_mut();
        assert_eq!(iter.size_hint(), (2, Some(2)));

        pairs.insert(iter.next().unwrap());
        assert_eq!(iter.size_hint(), (1, Some(1)));

        pairs.insert(iter.next().unwrap());
        assert_eq!(iter.size_hint(), (0, Some(0)));

        assert_eq!(iter.next(), None);
        assert_eq!(iter.next(), None);
        assert_eq!(iter.size_hint(), (0, Some(0)));

        assert!(pairs.contains(&(one, &mut 1)));
        assert!(pairs.contains(&(two, &mut 2)));
    }
}
