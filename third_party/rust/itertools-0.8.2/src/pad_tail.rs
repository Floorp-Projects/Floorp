use std::iter::Fuse;
use size_hint;

/// An iterator adaptor that pads a sequence to a minimum length by filling
/// missing elements using a closure.
///
/// Iterator element type is `I::Item`.
///
/// See [`.pad_using()`](../trait.Itertools.html#method.pad_using) for more information.
#[derive(Clone)]
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct PadUsing<I, F> {
    iter: Fuse<I>,
    min: usize,
    pos: usize,
    filler: F,
}

/// Create a new **PadUsing** iterator.
pub fn pad_using<I, F>(iter: I, min: usize, filler: F) -> PadUsing<I, F>
    where I: Iterator,
          F: FnMut(usize) -> I::Item
{
    PadUsing {
        iter: iter.fuse(),
        min: min,
        pos: 0,
        filler: filler,
    }
}

impl<I, F> Iterator for PadUsing<I, F>
    where I: Iterator,
          F: FnMut(usize) -> I::Item
{
    type Item = I::Item;

    #[inline]
    fn next(&mut self) -> Option<I::Item> {
        match self.iter.next() {
            None => {
                if self.pos < self.min {
                    let e = Some((self.filler)(self.pos));
                    self.pos += 1;
                    e
                } else {
                    None
                }
            },
            e => {
                self.pos += 1;
                e
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let tail = self.min.saturating_sub(self.pos);
        size_hint::max(self.iter.size_hint(), (tail, Some(tail)))
    }
}

impl<I, F> DoubleEndedIterator for PadUsing<I, F>
    where I: DoubleEndedIterator + ExactSizeIterator,
          F: FnMut(usize) -> I::Item
{
    fn next_back(&mut self) -> Option<I::Item> {
        if self.min == 0 {
            self.iter.next_back()
        } else if self.iter.len() >= self.min {
            self.min -= 1;
            self.iter.next_back()
        } else {
            self.min -= 1;
            Some((self.filler)(self.min))
        }
    }
}

impl<I, F> ExactSizeIterator for PadUsing<I, F>
    where I: ExactSizeIterator,
          F: FnMut(usize) -> I::Item
{}
