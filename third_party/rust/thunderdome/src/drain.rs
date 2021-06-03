use std::iter::{ExactSizeIterator, FusedIterator};

use crate::arena::{Arena, Index};

/// See [`Arena::drain`].
pub struct Drain<'a, T> {
    pub(crate) arena: &'a mut Arena<T>,
    pub(crate) slot: u32,
}

impl<'a, T> Iterator for Drain<'a, T> {
    type Item = (Index, T);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            // If there are no entries remaining in the arena, we should always
            // return None. Using this check instead of comparing with the
            // arena's size allows us to skip any trailing empty entries.
            if self.arena.is_empty() {
                return None;
            }

            // slot may overflow if the arena's underlying storage contains more
            // than 2^32 elements, but its internal length value was not
            // changed, as it overflowing would panic before reaching this code.
            let slot = self.slot;
            self.slot = self
                .slot
                .checked_add(1)
                .unwrap_or_else(|| panic!("Overflowed u32 trying to drain Arena"));

            // If this entry is occupied, this method will mark it as an empty.
            // Otherwise, we'll continue looping until we've drained all
            // occupied entries from the arena.
            if let Some((index, value)) = self.arena.remove_by_slot(slot) {
                return Some((index, value));
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.arena.len(), Some(self.arena.len()))
    }
}

impl<'a, T> FusedIterator for Drain<'a, T> {}
impl<'a, T> ExactSizeIterator for Drain<'a, T> {}

impl<'a, T> Drop for Drain<'a, T> {
    // Continue iterating/dropping if there are any elements left.
    fn drop(&mut self) {
        self.for_each(drop);
    }
}

#[cfg(test)]
mod test {
    use crate::Arena;

    use std::collections::HashSet;

    #[test]
    fn drain() {
        let mut arena = Arena::with_capacity(2);
        let one = arena.insert(1);
        let two = arena.insert(2);

        let mut drained_pairs = HashSet::new();
        {
            let mut drain = arena.drain();
            assert_eq!(drain.size_hint(), (2, Some(2)));

            drained_pairs.insert(drain.next().unwrap());
            assert_eq!(drain.size_hint(), (1, Some(1)));

            // Do not fully drain so we can ensure everything is dropped when the
            // `Drain` is dropped.
            assert_eq!(drain.size_hint(), (1, Some(1)));
        }

        assert_eq!(arena.len(), 0);
        assert_eq!(arena.capacity(), 2);
        assert_eq!(drained_pairs.len(), 1);

        // We should still be able to use the arena after this.
        let one_prime = arena.insert(1);
        let two_prime = arena.insert(2);

        assert_eq!(arena.len(), 2);
        assert_eq!(arena.capacity(), 2);
        assert_eq!(arena.get(one_prime), Some(&1));
        assert_eq!(arena.get(two_prime), Some(&2));
        assert_eq!(arena.get(one), None);
        assert_eq!(arena.get(two), None);
    }
}
