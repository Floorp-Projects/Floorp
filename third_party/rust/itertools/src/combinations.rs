
use std::ops::Index;
use std::fmt;

/// An iterator to iterate through all the `n`-length combinations in an iterator.
///
/// See [`.combinations()`](../trait.Itertools.html#method.combinations) for more information.
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct Combinations<I: Iterator> {
    n: usize,
    indices: Vec<usize>,
    pool: LazyBuffer<I>,
    first: bool,
}

impl<I> fmt::Debug for Combinations<I>
    where I: Iterator + fmt::Debug,
          I::Item: fmt::Debug,
{
    debug_fmt_fields!(Combinations, n, indices, pool, first);
}

/// Create a new `Combinations` from a clonable iterator.
pub fn combinations<I>(iter: I, n: usize) -> Combinations<I>
    where I: Iterator
{
    let mut indices: Vec<usize> = Vec::with_capacity(n);
    for i in 0..n {
        indices.push(i);
    }
    let mut pool: LazyBuffer<I> = LazyBuffer::new(iter);

    for _ in 0..n {
        if !pool.get_next() {
            break;
        }
    }

    Combinations {
        n: n,
        indices: indices,
        pool: pool,
        first: true,
    }
}

impl<I> Iterator for Combinations<I>
    where I: Iterator,
          I::Item: Clone
{
    type Item = Vec<I::Item>;
    fn next(&mut self) -> Option<Self::Item> {
        let mut pool_len = self.pool.len();
        if self.pool.is_done() {
            if pool_len == 0 || self.n > pool_len {
                return None;
            }
        }

        if self.first {
            self.first = false;
        } else if self.n == 0 {
            return None;
        } else {
            // Scan from the end, looking for an index to increment
            let mut i: usize = self.n - 1;

            // Check if we need to consume more from the iterator
            if self.indices[i] == pool_len - 1 && !self.pool.is_done() {
                if self.pool.get_next() {
                    pool_len += 1;
                }
            }

            while self.indices[i] == i + pool_len - self.n {
                if i > 0 {
                    i -= 1;
                } else {
                    // Reached the last combination
                    return None;
                }
            }

            // Increment index, and reset the ones to its right
            self.indices[i] += 1;
            let mut j = i + 1;
            while j < self.n {
                self.indices[j] = self.indices[j - 1] + 1;
                j += 1;
            }
        }

        // Create result vector based on the indices
        let mut result = Vec::with_capacity(self.n);
        for i in self.indices.iter() {
            result.push(self.pool[*i].clone());
        }
        Some(result)
    }
}

#[derive(Debug)]
struct LazyBuffer<I: Iterator> {
    it: I,
    done: bool,
    buffer: Vec<I::Item>,
}

impl<I> LazyBuffer<I>
    where I: Iterator
{
    pub fn new(it: I) -> LazyBuffer<I> {
        let mut it = it;
        let mut buffer = Vec::new();
        let done;
        if let Some(first) = it.next() {
            buffer.push(first);
            done = false;
        } else {
            done = true;
        }
        LazyBuffer {
            it: it,
            done: done,
            buffer: buffer,
        }
    }

    pub fn len(&self) -> usize {
        self.buffer.len()
    }

    pub fn is_done(&self) -> bool {
        self.done
    }

    pub fn get_next(&mut self) -> bool {
        if self.done {
            return false;
        }
        let next_item = self.it.next();
        match next_item {
            Some(x) => {
                self.buffer.push(x);
                true
            }
            None => {
                self.done = true;
                false
            }
        }
    }
}

impl<I> Index<usize> for LazyBuffer<I>
    where I: Iterator,
          I::Item: Sized
{
    type Output = I::Item;

    fn index<'b>(&'b self, _index: usize) -> &'b I::Item {
        self.buffer.index(_index)
    }
}

