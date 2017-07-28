use std::cmp::Ordering::{Equal, Greater, Less};
use super::size_hint;
use std::iter::Fuse;
use self::EitherOrBoth::{Right, Left, Both};

// ZipLongest originally written by SimonSapin,
// and dedicated to itertools https://github.com/rust-lang/rust/pull/19283

/// An iterator which iterates two other iterators simultaneously
///
/// This iterator is *fused*.
///
/// See [`.zip_longest()`](../trait.Itertools.html#method.zip_longest) for more information.
#[derive(Clone)]
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
pub struct ZipLongest<T, U> {
    a: Fuse<T>,
    b: Fuse<U>,
}

/// Create a new `ZipLongest` iterator.
pub fn zip_longest<T, U>(a: T, b: U) -> ZipLongest<T, U> 
    where T: Iterator,
          U: Iterator
{
    ZipLongest {
        a: a.fuse(),
        b: b.fuse(),
    }
}

impl<T, U> Iterator for ZipLongest<T, U>
    where T: Iterator,
          U: Iterator
{
    type Item = EitherOrBoth<T::Item, U::Item>;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        match (self.a.next(), self.b.next()) {
            (None, None) => None,
            (Some(a), None) => Some(Left(a)),
            (None, Some(b)) => Some(Right(b)),
            (Some(a), Some(b)) => Some(Both(a, b)),
        }
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        size_hint::max(self.a.size_hint(), self.b.size_hint())
    }
}

impl<T, U> DoubleEndedIterator for ZipLongest<T, U>
    where T: DoubleEndedIterator + ExactSizeIterator,
          U: DoubleEndedIterator + ExactSizeIterator
{
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        match self.a.len().cmp(&self.b.len()) {
            Equal => match (self.a.next_back(), self.b.next_back()) {
                (None, None) => None,
                (Some(a), Some(b)) => Some(Both(a, b)),
                // These can only happen if .len() is inconsistent with .next_back()
                (Some(a), None) => Some(Left(a)),
                (None, Some(b)) => Some(Right(b)),
            },
            Greater => self.a.next_back().map(Left),
            Less => self.b.next_back().map(Right),
        }
    }
}

impl<T, U> ExactSizeIterator for ZipLongest<T, U>
    where T: ExactSizeIterator,
          U: ExactSizeIterator
{}


/// A value yielded by `ZipLongest`.
/// Contains one or two values, depending on which of the input iterators are exhausted.
///
/// See [`.zip_longest()`](trait.Itertools.html#method.zip_longest) for more information.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum EitherOrBoth<A, B> {
    /// Neither input iterator is exhausted yet, yielding two values.
    Both(A, B),
    /// The parameter iterator of `.zip_longest()` is exhausted,
    /// only yielding a value from the `self` iterator.
    Left(A),
    /// The `self` iterator of `.zip_longest()` is exhausted,
    /// only yielding a value from the parameter iterator.
    Right(B),
}
