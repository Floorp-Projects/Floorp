//! Free functions that create iterator adaptors or call iterator methods.
//!
//! The benefit of free functions is that they accept any `IntoIterator` as
//! argument, so the resulting code may be easier to read.

#[cfg(feature = "use_std")]
use std::fmt::Display;
use std::iter::{self, Zip};
#[cfg(feature = "use_std")]
type VecIntoIter<T> = ::std::vec::IntoIter<T>;

#[cfg(feature = "use_std")]
use Itertools;

pub use adaptors::{
    interleave,
    merge,
    put_back,
};
#[cfg(feature = "use_std")]
pub use put_back_n_impl::put_back_n;
#[cfg(feature = "use_std")]
pub use multipeek_impl::multipeek;
#[cfg(feature = "use_std")]
pub use kmerge_impl::kmerge;
pub use zip_eq_impl::zip_eq;
pub use merge_join::merge_join_by;
#[cfg(feature = "use_std")]
pub use rciter_impl::rciter;

/// Iterate `iterable` with a running index.
///
/// `IntoIterator` enabled version of `.enumerate()`.
///
/// ```
/// use itertools::enumerate;
///
/// for (i, elt) in enumerate(&[1, 2, 3]) {
///     /* loop body */
/// }
/// ```
pub fn enumerate<I>(iterable: I) -> iter::Enumerate<I::IntoIter>
    where I: IntoIterator
{
    iterable.into_iter().enumerate()
}

/// Iterate `iterable` in reverse.
///
/// `IntoIterator` enabled version of `.rev()`.
///
/// ```
/// use itertools::rev;
///
/// for elt in rev(&[1, 2, 3]) {
///     /* loop body */
/// }
/// ```
pub fn rev<I>(iterable: I) -> iter::Rev<I::IntoIter>
    where I: IntoIterator,
          I::IntoIter: DoubleEndedIterator
{
    iterable.into_iter().rev()
}

/// Iterate `i` and `j` in lock step.
///
/// `IntoIterator` enabled version of `i.zip(j)`.
///
/// ```
/// use itertools::zip;
///
/// let data = [1, 2, 3, 4, 5];
/// for (a, b) in zip(&data, &data[1..]) {
///     /* loop body */
/// }
/// ```
pub fn zip<I, J>(i: I, j: J) -> Zip<I::IntoIter, J::IntoIter>
    where I: IntoIterator,
          J: IntoIterator
{
    i.into_iter().zip(j)
}

/// Create an iterator that first iterates `i` and then `j`.
///
/// `IntoIterator` enabled version of `i.chain(j)`.
///
/// ```
/// use itertools::chain;
///
/// for elt in chain(&[1, 2, 3], &[4]) {
///     /* loop body */
/// }
/// ```
pub fn chain<I, J>(i: I, j: J) -> iter::Chain<<I as IntoIterator>::IntoIter, <J as IntoIterator>::IntoIter>
    where I: IntoIterator,
          J: IntoIterator<Item = I::Item>
{
    i.into_iter().chain(j)
}

/// Create an iterator that clones each element from &T to T
///
/// `IntoIterator` enabled version of `i.cloned()`.
///
/// ```
/// use itertools::cloned;
///
/// assert_eq!(cloned(b"abc").next(), Some(b'a'));
/// ```
pub fn cloned<'a, I, T: 'a>(iterable: I) -> iter::Cloned<I::IntoIter>
    where I: IntoIterator<Item=&'a T>,
          T: Clone,
{
    iterable.into_iter().cloned()
}

/// Perform a fold operation over the iterable.
///
/// `IntoIterator` enabled version of `i.fold(init, f)`
///
/// ```
/// use itertools::fold;
///
/// assert_eq!(fold(&[1., 2., 3.], 0., |a, &b| f32::max(a, b)), 3.);
/// ```
pub fn fold<I, B, F>(iterable: I, init: B, f: F) -> B
    where I: IntoIterator,
          F: FnMut(B, I::Item) -> B
{
    iterable.into_iter().fold(init, f)
}

/// Test whether the predicate holds for all elements in the iterable.
///
/// `IntoIterator` enabled version of `i.all(f)`
///
/// ```
/// use itertools::all;
///
/// assert!(all(&[1, 2, 3], |elt| *elt > 0));
/// ```
pub fn all<I, F>(iterable: I, f: F) -> bool
    where I: IntoIterator,
          F: FnMut(I::Item) -> bool
{
    iterable.into_iter().all(f)
}

/// Test whether the predicate holds for any elements in the iterable.
///
/// `IntoIterator` enabled version of `i.any(f)`
///
/// ```
/// use itertools::any;
///
/// assert!(any(&[0, -1, 2], |elt| *elt > 0));
/// ```
pub fn any<I, F>(iterable: I, f: F) -> bool
    where I: IntoIterator,
          F: FnMut(I::Item) -> bool
{
    iterable.into_iter().any(f)
}

/// Return the maximum value of the iterable.
///
/// `IntoIterator` enabled version of `i.max()`.
///
/// ```
/// use itertools::max;
///
/// assert_eq!(max(0..10), Some(9));
/// ```
pub fn max<I>(iterable: I) -> Option<I::Item>
    where I: IntoIterator,
          I::Item: Ord
{
    iterable.into_iter().max()
}

/// Return the minimum value of the iterable.
///
/// `IntoIterator` enabled version of `i.min()`.
///
/// ```
/// use itertools::min;
///
/// assert_eq!(min(0..10), Some(0));
/// ```
pub fn min<I>(iterable: I) -> Option<I::Item>
    where I: IntoIterator,
          I::Item: Ord
{
    iterable.into_iter().min()
}


/// Combine all iterator elements into one String, seperated by `sep`.
///
/// `IntoIterator` enabled version of `iterable.join(sep)`.
///
/// ```
/// use itertools::join;
///
/// assert_eq!(join(&[1, 2, 3], ", "), "1, 2, 3");
/// ```
#[cfg(feature = "use_std")]
pub fn join<I>(iterable: I, sep: &str) -> String
    where I: IntoIterator,
          I::Item: Display
{
    iterable.into_iter().join(sep)
}

/// Sort all iterator elements into a new iterator in ascending order.
///
/// `IntoIterator` enabled version of [`iterable.sorted()`][1].
///
/// [1]: trait.Itertools.html#method.sorted
///
/// ```
/// use itertools::sorted;
/// use itertools::assert_equal;
///
/// assert_equal(sorted("rust".chars()), "rstu".chars());
/// ```
#[cfg(feature = "use_std")]
pub fn sorted<I>(iterable: I) -> VecIntoIter<I::Item>
    where I: IntoIterator,
          I::Item: Ord
{
    iterable.into_iter().sorted()
}

