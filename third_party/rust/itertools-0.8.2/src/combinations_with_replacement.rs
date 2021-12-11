use std::fmt;

use super::lazy_buffer::LazyBuffer;

/// An iterator to iterate through all the `n`-length combinations in an iterator, with replacement.
///
/// See [`.combinations_with_replacement()`](../trait.Itertools.html#method.combinations_with_replacement) for more information.
#[derive(Clone)]
pub struct CombinationsWithReplacement<I>
where
    I: Iterator,
    I::Item: Clone,
{
    k: usize,
    indices: Vec<usize>,
    // The current known max index value. This increases as pool grows.
    max_index: usize,
    pool: LazyBuffer<I>,
    first: bool,
}

impl<I> fmt::Debug for CombinationsWithReplacement<I>
where
    I: Iterator + fmt::Debug,
    I::Item: fmt::Debug + Clone,
{
    debug_fmt_fields!(Combinations, k, indices, max_index, pool, first);
}

impl<I> CombinationsWithReplacement<I>
where
    I: Iterator,
    I::Item: Clone,
{
    /// Map the current mask over the pool to get an output combination
    fn current(&self) -> Vec<I::Item> {
        self.indices.iter().map(|i| self.pool[*i].clone()).collect()
    }
}

/// Create a new `CombinationsWithReplacement` from a clonable iterator.
pub fn combinations_with_replacement<I>(iter: I, k: usize) -> CombinationsWithReplacement<I>
where
    I: Iterator,
    I::Item: Clone,
{
    let indices: Vec<usize> = vec![0; k];
    let pool: LazyBuffer<I> = LazyBuffer::new(iter);

    CombinationsWithReplacement {
        k,
        indices,
        max_index: 0,
        pool: pool,
        first: true,
    }
}

impl<I> Iterator for CombinationsWithReplacement<I>
where
    I: Iterator,
    I::Item: Clone,
{
    type Item = Vec<I::Item>;
    fn next(&mut self) -> Option<Self::Item> {
        // If this is the first iteration, return early
        if self.first {
            // In empty edge cases, stop iterating immediately
            return if self.k == 0 || self.pool.is_done() {
                None
            // Otherwise, yield the initial state
            } else {
                self.first = false;
                Some(self.current())
            };
        }

        // Check if we need to consume more from the iterator
        // This will run while we increment our first index digit
        if !self.pool.is_done() {
            self.pool.get_next();
            self.max_index = self.pool.len() - 1;
        }

        // Work out where we need to update our indices
        let mut increment: Option<(usize, usize)> = None;
        for (i, indices_int) in self.indices.iter().enumerate().rev() {
            if indices_int < &self.max_index {
                increment = Some((i, indices_int + 1));
                break;
            }
        }

        match increment {
            // If we can update the indices further
            Some((increment_from, increment_value)) => {
                // We need to update the rightmost non-max value
                // and all those to the right
                for indices_index in increment_from..self.indices.len() {
                    self.indices[indices_index] = increment_value
                }
                Some(self.current())
            }
            // Otherwise, we're done
            None => None,
        }
    }
}
