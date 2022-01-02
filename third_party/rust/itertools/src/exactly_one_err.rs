#[cfg(feature = "use_std")]
use std::error::Error;
use std::fmt::{Debug, Display, Formatter, Result as FmtResult};

use std::iter::ExactSizeIterator;

use either::Either;

use crate::size_hint;

/// Iterator returned for the error case of `IterTools::exactly_one()`
/// This iterator yields exactly the same elements as the input iterator.
///
/// During the execution of exactly_one the iterator must be mutated.  This wrapper
/// effectively "restores" the state of the input iterator when it's handed back.
///
/// This is very similar to PutBackN except this iterator only supports 0-2 elements and does not
/// use a `Vec`.
#[derive(Clone)]
pub struct ExactlyOneError<I>
where
    I: Iterator,
{
    first_two: Option<Either<[I::Item; 2], I::Item>>,
    inner: I,
}

impl<I> ExactlyOneError<I>
where
    I: Iterator,
{
    /// Creates a new `ExactlyOneErr` iterator.
    pub(crate) fn new(first_two: Option<Either<[I::Item; 2], I::Item>>, inner: I) -> Self {
        Self { first_two, inner }
    }

    fn additional_len(&self) -> usize {
        match self.first_two {
            Some(Either::Left(_)) => 2,
            Some(Either::Right(_)) => 1,
            None => 0,
        }
    }
}

impl<I> Iterator for ExactlyOneError<I>
where
    I: Iterator,
{
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        match self.first_two.take() {
            Some(Either::Left([first, second])) => {
                self.first_two = Some(Either::Right(second));
                Some(first)
            },
            Some(Either::Right(second)) => {
                Some(second)
            }
            None => {
                self.inner.next()
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        size_hint::add_scalar(self.inner.size_hint(), self.additional_len())
    }
}


impl<I> ExactSizeIterator for ExactlyOneError<I> where I: ExactSizeIterator {}

impl<I> Display for ExactlyOneError<I> 
    where I: Iterator,
{
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        let additional = self.additional_len();
        if additional > 0 {
            write!(f, "got at least 2 elements when exactly one was expected")
        } else {
            write!(f, "got zero elements when exactly one was expected")
        }
    }
}

impl<I> Debug for ExactlyOneError<I> 
    where I: Iterator + Debug,
          I::Item: Debug,
{
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        match &self.first_two {
            Some(Either::Left([first, second])) => {
                write!(f, "ExactlyOneError[First: {:?}, Second: {:?}, RemainingIter: {:?}]", first, second, self.inner)
            },
            Some(Either::Right(second)) => {
                write!(f, "ExactlyOneError[Second: {:?}, RemainingIter: {:?}]", second, self.inner)
            }
            None => {
                write!(f, "ExactlyOneError[RemainingIter: {:?}]", self.inner)
            }
        }
    }
}

#[cfg(feature = "use_std")]
impl<I> Error for ExactlyOneError<I>  where I: Iterator + Debug, I::Item: Debug, {}


