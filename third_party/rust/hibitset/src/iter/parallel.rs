use rayon::iter::plumbing::{bridge_unindexed, Folder, UnindexedConsumer, UnindexedProducer};
use rayon::iter::ParallelIterator;

use iter::{BitIter, BitSetLike, Index, BITS, LAYERS};
use util::average_ones;

/// A `ParallelIterator` over a [`BitSetLike`] structure.
///
/// [`BitSetLike`]: ../../trait.BitSetLike.html
#[derive(Debug)]
pub struct BitParIter<T>(T, u8);

impl<T> BitParIter<T> {
    /// Creates a new `BitParIter`. You usually don't call this function
    /// but just [`.par_iter()`] on a bit set.
    ///
    /// Default layer split amount is 3.
    ///
    /// [`.par_iter()`]: ../../trait.BitSetLike.html#method.par_iter
    pub fn new(set: T) -> Self {
        BitParIter(set, 3)
    }

    /// Sets how many layers are split when forking.
    ///
    /// # Examples
    ///
    /// ```
    /// # extern crate rayon;
    /// # extern crate hibitset;
    /// # use hibitset::{BitSet, BitSetLike};
    /// # use rayon::iter::ParallelIterator;
    /// # fn main() {
    /// let mut bitset = BitSet::new();
    /// bitset.par_iter()
    ///     .layers_split(2)
    ///     .count();
    /// # }
    /// ```
    ///
    /// The value should be in range [1, 3]
    ///
    /// | splits | largest smallest unit of work |
    /// |--------|-------------------------------|
    /// | 1      | usize_bits<sup>3</sup>        |
    /// | 2      | usize_bits<sup>2</sup>        |
    /// | 3      | usize_bits                    |
    ///
    pub fn layers_split(mut self, layers: u8) -> Self {
        assert!(layers >= 1);
        assert!(layers <= 3);
        self.1 = layers;
        self
    }
}

impl<T> ParallelIterator for BitParIter<T>
where
    T: BitSetLike + Send + Sync,
{
    type Item = Index;

    fn drive_unindexed<C>(self, consumer: C) -> C::Result
    where
        C: UnindexedConsumer<Self::Item>,
    {
        bridge_unindexed(BitProducer((&self.0).iter(), self.1), consumer)
    }
}

/// Allows splitting and internally iterating through `BitSet`.
///
/// Usually used internally by `BitParIter`.
#[derive(Debug)]
pub struct BitProducer<'a, T: 'a + Send + Sync>(pub BitIter<&'a T>, pub u8);

impl<'a, T: 'a + Send + Sync> UnindexedProducer for BitProducer<'a, T>
where
    T: BitSetLike,
{
    type Item = Index;

    /// How the splitting is done:
    ///
    /// 1) First the highest layer that has at least one set bit
    ///    is searched.
    ///
    /// 2) If the layer that was found, has only one bit that's set,
    ///    it's cleared. After that the correct prefix for the cleared
    ///    bit is figured out and the descending is continued.
    ///
    /// 3) If the layer that was found, has more than one bit that's set,
    ///    a mask is created that splits it's set bits as close to half
    ///    as possible.
    ///    After creating the mask the layer is masked by either the mask
    ///    or it's complement constructing two distinct producers which
    ///    are then returned.
    ///
    /// 4) If there isn't any layers that have more than one set bit,
    ///    splitting doesn't happen.
    ///
    /// The actual iteration is performed by the sequential iterator
    /// `BitIter` which internals are modified by this splitting
    ///  algorithm.
    ///
    /// This splitting strategy should split work evenly if the set bits
    /// are distributed close to uniformly random.
    /// As the strategy only looks one layer at the time, if there are subtrees
    /// that have lots of work and sibling subtrees that have little of work,
    /// then it will produce non-optimal splittings.
    fn split(mut self) -> (Self, Option<Self>) {
        let splits = self.1;
        let other = {
            let mut handle_level = |level: usize| {
                if self.0.masks[level] == 0 {
                    // Skip the empty layers
                    None
                } else {
                    // Top levels prefix is zero because there is nothing before it
                    let level_prefix = self.0.prefix.get(level).cloned().unwrap_or(0);
                    let first_bit = self.0.masks[level].trailing_zeros();
                    average_ones(self.0.masks[level])
                        .and_then(|average_bit| {
                            let mask = (1 << average_bit) - 1;
                            let mut other = BitProducer(
                                BitIter::new(self.0.set, [0; LAYERS], [0; LAYERS - 1]),
                                splits,
                            );
                            // The `other` is the more significant half of the mask
                            other.0.masks[level] = self.0.masks[level] & !mask;
                            other.0.prefix[level - 1] = (level_prefix | average_bit as u32) << BITS;
                            // The upper portion of the prefix is maintained, because the `other`
                            // will iterate the same subtree as the `self` does
                            other.0.prefix[level..].copy_from_slice(&self.0.prefix[level..]);
                            // And the `self` is the less significant one
                            self.0.masks[level] &= mask;
                            self.0.prefix[level - 1] = (level_prefix | first_bit) << BITS;
                            Some(other)
                        })
                        .or_else(|| {
                            // Because there is only one bit left we descend to it
                            let idx = level_prefix as usize | first_bit as usize;
                            self.0.prefix[level - 1] = (idx as u32) << BITS;
                            // The level that is descended from doesn't have anything
                            // interesting so it can be skipped in the future.
                            self.0.masks[level] = 0;
                            self.0.masks[level - 1] = self.0.set.get_from_layer(level - 1, idx);
                            None
                        })
                }
            };
            let top_layer = LAYERS - 1;
            let mut h = handle_level(top_layer);
            for i in 1..splits {
                h = h.or_else(|| handle_level(top_layer - i as usize));
            }
            h
        };
        (self, other)
    }

    fn fold_with<F>(self, folder: F) -> F
    where
        F: Folder<Self::Item>,
    {
        folder.consume_iter(self.0)
    }
}

#[cfg(test)]
mod test_bit_producer {
    use rayon::iter::plumbing::UnindexedProducer;

    use super::BitProducer;
    use iter::BitSetLike;
    use util::BITS;

    fn test_splitting(split_levels: u8) {
        fn visit<T>(mut us: BitProducer<T>, d: usize, i: usize, mut trail: String, c: &mut usize)
        where
            T: Send + Sync + BitSetLike,
        {
            if d == 0 {
                assert!(us.split().1.is_none(), trail);
                *c += 1;
            } else {
                for j in 1..(i + 1) {
                    let (new_us, them) = us.split();
                    us = new_us;
                    let them = them.expect(&trail);
                    let mut trail = trail.clone();
                    trail.push_str(&i.to_string());
                    visit(them, d, i - j, trail, c);
                }
                trail.push_str("u");
                visit(us, d - 1, BITS, trail, c);
            }
        }

        let usize_bits = ::std::mem::size_of::<usize>() * 8;

        let mut c = ::BitSet::new();
        for i in 0..(usize_bits.pow(3) * 2) {
            assert!(!c.add(i as u32));
        }

        let us = BitProducer((&c).iter(), split_levels);
        let (us, them) = us.split();

        let mut count = 0;
        visit(
            us,
            split_levels as usize - 1,
            BITS,
            "u".to_owned(),
            &mut count,
        );
        visit(
            them.expect("Splitting top level"),
            split_levels as usize - 1,
            BITS,
            "t".to_owned(),
            &mut count,
        );
        assert_eq!(usize_bits.pow(split_levels as u32 - 1) * 2, count);
    }

    #[test]
    fn max_3_splitting_of_two_top_bits() {
        test_splitting(3);
    }

    #[test]
    fn max_2_splitting_of_two_top_bits() {
        test_splitting(2);
    }

    #[test]
    fn max_1_splitting_of_two_top_bits() {
        test_splitting(1);
    }
}
