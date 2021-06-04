use std::iter::{ExactSizeIterator, FusedIterator};

use crate::arena::{Arena, Index};

/// Iterator typed used when an Arena is turned [`IntoIterator`].
pub struct IntoIter<T> {
    pub(crate) arena: Arena<T>,
    pub(crate) slot: u32,
}

impl<T> Iterator for IntoIter<T> {
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
                .unwrap_or_else(|| panic!("Overflowed u32 trying to into_iter Arena"));

            // If this entry is occupied, this method will mark it as an empty.
            // Otherwise, we'll continue looping until we've removed all
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

impl<T> FusedIterator for IntoIter<T> {}
impl<T> ExactSizeIterator for IntoIter<T> {}

#[cfg(test)]
mod test {
    use crate::Arena;

    use std::collections::HashSet;

    #[test]
    fn into_iter() {
        let mut arena = Arena::with_capacity(2);
        let one = arena.insert(1);
        let two = arena.insert(2);

        let mut pairs = HashSet::new();
        let mut into_iter = arena.into_iter();
        assert_eq!(into_iter.size_hint(), (2, Some(2)));

        pairs.insert(into_iter.next().unwrap());
        assert_eq!(into_iter.size_hint(), (1, Some(1)));

        pairs.insert(into_iter.next().unwrap());
        assert_eq!(into_iter.size_hint(), (0, Some(0)));

        assert_eq!(into_iter.next(), None);
        assert_eq!(into_iter.next(), None);
        assert_eq!(into_iter.size_hint(), (0, Some(0)));

        assert_eq!(pairs.len(), 2);
        assert!(pairs.contains(&(one, 1)));
        assert!(pairs.contains(&(two, 2)));
    }
}
