// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use Rng;
use distributions::Distribution;
use distributions::uniform::{UniformSampler, SampleUniform, SampleBorrow};
use ::core::cmp::PartialOrd;
use core::fmt;

// Note that this whole module is only imported if feature="alloc" is enabled.
#[cfg(not(feature="std"))] use alloc::vec::Vec;

/// A distribution using weighted sampling to pick a discretely selected
/// item.
///
/// Sampling a `WeightedIndex` distribution returns the index of a randomly
/// selected element from the iterator used when the `WeightedIndex` was
/// created. The chance of a given element being picked is proportional to the
/// value of the element. The weights can use any type `X` for which an
/// implementation of [`Uniform<X>`] exists.
///
/// # Performance
///
/// A `WeightedIndex<X>` contains a `Vec<X>` and a [`Uniform<X>`] and so its
/// size is the sum of the size of those objects, possibly plus some alignment.
///
/// Creating a `WeightedIndex<X>` will allocate enough space to hold `N - 1`
/// weights of type `X`, where `N` is the number of weights. However, since
/// `Vec` doesn't guarantee a particular growth strategy, additional memory
/// might be allocated but not used. Since the `WeightedIndex` object also
/// contains, this might cause additional allocations, though for primitive
/// types, ['Uniform<X>`] doesn't allocate any memory.
///
/// Time complexity of sampling from `WeightedIndex` is `O(log N)` where
/// `N` is the number of weights.
///
/// Sampling from `WeightedIndex` will result in a single call to
/// `Uniform<X>::sample` (method of the [`Distribution`] trait), which typically
/// will request a single value from the underlying [`RngCore`], though the
/// exact number depends on the implementaiton of `Uniform<X>::sample`.
///
/// # Example
///
/// ```
/// use rand::prelude::*;
/// use rand::distributions::WeightedIndex;
///
/// let choices = ['a', 'b', 'c'];
/// let weights = [2,   1,   1];
/// let dist = WeightedIndex::new(&weights).unwrap();
/// let mut rng = thread_rng();
/// for _ in 0..100 {
///     // 50% chance to print 'a', 25% chance to print 'b', 25% chance to print 'c'
///     println!("{}", choices[dist.sample(&mut rng)]);
/// }
///
/// let items = [('a', 0), ('b', 3), ('c', 7)];
/// let dist2 = WeightedIndex::new(items.iter().map(|item| item.1)).unwrap();
/// for _ in 0..100 {
///     // 0% chance to print 'a', 30% chance to print 'b', 70% chance to print 'c'
///     println!("{}", items[dist2.sample(&mut rng)].0);
/// }
/// ```
///
/// [`Uniform<X>`]: crate::distributions::uniform::Uniform
/// [`RngCore`]: rand_core::RngCore
#[derive(Debug, Clone)]
pub struct WeightedIndex<X: SampleUniform + PartialOrd> {
    cumulative_weights: Vec<X>,
    weight_distribution: X::Sampler,
}

impl<X: SampleUniform + PartialOrd> WeightedIndex<X> {
    /// Creates a new a `WeightedIndex` [`Distribution`] using the values
    /// in `weights`. The weights can use any type `X` for which an
    /// implementation of [`Uniform<X>`] exists.
    ///
    /// Returns an error if the iterator is empty, if any weight is `< 0`, or
    /// if its total value is 0.
    ///
    /// [`Uniform<X>`]: crate::distributions::uniform::Uniform
    pub fn new<I>(weights: I) -> Result<WeightedIndex<X>, WeightedError>
        where I: IntoIterator,
              I::Item: SampleBorrow<X>,
              X: for<'a> ::core::ops::AddAssign<&'a X> +
                 Clone +
                 Default {
        let mut iter = weights.into_iter();
        let mut total_weight: X = iter.next()
                                      .ok_or(WeightedError::NoItem)?
                                      .borrow()
                                      .clone();

        let zero = <X as Default>::default();
        if total_weight < zero {
            return Err(WeightedError::NegativeWeight);
        }

        let mut weights = Vec::<X>::with_capacity(iter.size_hint().0);
        for w in iter {
            if *w.borrow() < zero {
                return Err(WeightedError::NegativeWeight);
            }
            weights.push(total_weight.clone());
            total_weight += w.borrow();
        }

        if total_weight == zero {
            return Err(WeightedError::AllWeightsZero);
        }
        let distr = X::Sampler::new(zero, total_weight);

        Ok(WeightedIndex { cumulative_weights: weights, weight_distribution: distr })
    }
}

impl<X> Distribution<usize> for WeightedIndex<X> where
    X: SampleUniform + PartialOrd {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> usize {
        use ::core::cmp::Ordering;
        let chosen_weight = self.weight_distribution.sample(rng);
        // Find the first item which has a weight *higher* than the chosen weight.
        self.cumulative_weights.binary_search_by(
            |w| if *w <= chosen_weight { Ordering::Less } else { Ordering::Greater }).unwrap_err()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_weightedindex() {
        let mut r = ::test::rng(700);
        const N_REPS: u32 = 5000;
        let weights = [1u32, 2, 3, 0, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7];
        let total_weight = weights.iter().sum::<u32>() as f32;

        let verify = |result: [i32; 14]| {
            for (i, count) in result.iter().enumerate() {
                let exp = (weights[i] * N_REPS) as f32 / total_weight;
                let mut err = (*count as f32 - exp).abs();
                if err != 0.0 {
                    err /= exp;
                }
                assert!(err <= 0.25);
            }
        };

        // WeightedIndex from vec
        let mut chosen = [0i32; 14];
        let distr = WeightedIndex::new(weights.to_vec()).unwrap();
        for _ in 0..N_REPS {
            chosen[distr.sample(&mut r)] += 1;
        }
        verify(chosen);

        // WeightedIndex from slice
        chosen = [0i32; 14];
        let distr = WeightedIndex::new(&weights[..]).unwrap();
        for _ in 0..N_REPS {
            chosen[distr.sample(&mut r)] += 1;
        }
        verify(chosen);

        // WeightedIndex from iterator
        chosen = [0i32; 14];
        let distr = WeightedIndex::new(weights.iter()).unwrap();
        for _ in 0..N_REPS {
            chosen[distr.sample(&mut r)] += 1;
        }
        verify(chosen);

        for _ in 0..5 {
            assert_eq!(WeightedIndex::new(&[0, 1]).unwrap().sample(&mut r), 1);
            assert_eq!(WeightedIndex::new(&[1, 0]).unwrap().sample(&mut r), 0);
            assert_eq!(WeightedIndex::new(&[0, 0, 0, 0, 10, 0]).unwrap().sample(&mut r), 4);
        }

        assert_eq!(WeightedIndex::new(&[10][0..0]).unwrap_err(), WeightedError::NoItem);
        assert_eq!(WeightedIndex::new(&[0]).unwrap_err(), WeightedError::AllWeightsZero);
        assert_eq!(WeightedIndex::new(&[10, 20, -1, 30]).unwrap_err(), WeightedError::NegativeWeight);
        assert_eq!(WeightedIndex::new(&[-10, 20, 1, 30]).unwrap_err(), WeightedError::NegativeWeight);
        assert_eq!(WeightedIndex::new(&[-10]).unwrap_err(), WeightedError::NegativeWeight);
    }
}

/// Error type returned from `WeightedIndex::new`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WeightedError {
    /// The provided iterator contained no items.
    NoItem,

    /// A weight lower than zero was used.
    NegativeWeight,

    /// All items in the provided iterator had a weight of zero.
    AllWeightsZero,
}

impl WeightedError {
    fn msg(&self) -> &str {
        match *self {
            WeightedError::NoItem => "No items found",
            WeightedError::NegativeWeight => "Item has negative weight",
            WeightedError::AllWeightsZero => "All items had weight zero",
        }
    }
}

#[cfg(feature="std")]
impl ::std::error::Error for WeightedError {
    fn description(&self) -> &str {
        self.msg()
    }
    fn cause(&self) -> Option<&::std::error::Error> {
        None
    }
}

impl fmt::Display for WeightedError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.msg())
    }
}
