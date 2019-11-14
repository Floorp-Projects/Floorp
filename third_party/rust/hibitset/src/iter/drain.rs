use iter::BitIter;
use util::*;
use DrainableBitSet;

/// A draining `Iterator` over a [`DrainableBitSet`] structure.
///
/// [`DrainableBitSet`]: ../trait.DrainableBitSet.html
pub struct DrainBitIter<'a, T: 'a> {
    iter: BitIter<&'a mut T>,
}

impl<'a, T: DrainableBitSet> DrainBitIter<'a, T> {
    /// Creates a new `DrainBitIter`. You usually don't call this function
    /// but just [`.drain()`] on a bit set.
    ///
    /// [`.drain()`]: ../trait.DrainableBitSet.html#method.drain
    pub fn new(set: &'a mut T, masks: [usize; LAYERS], prefix: [u32; LAYERS - 1]) -> Self {
        DrainBitIter {
            iter: BitIter::new(set, masks, prefix),
        }
    }
}

impl<'a, T> Iterator for DrainBitIter<'a, T>
where
    T: DrainableBitSet,
{
    type Item = Index;

    fn next(&mut self) -> Option<Self::Item> {
        let next = self.iter.next();
        if let Some(next) = next {
            self.iter.set.remove(next);
        }
        next
    }
}

#[test]
fn drain_all() {
    use {BitSet, BitSetLike};
    let mut bit_set: BitSet = (0..10000).filter(|i| i % 2 == 0).collect();
    bit_set.drain().for_each(|_| {});
    assert_eq!(0, bit_set.iter().count());
}
