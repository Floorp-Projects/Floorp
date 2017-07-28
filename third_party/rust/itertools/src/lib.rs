#![warn(missing_docs)]
#![crate_name="itertools"]

//! Itertools — extra iterator adaptors, functions and macros.
//!
//! To use the iterator methods in this crate, import the [`Itertools` trait](./trait.Itertools.html):
//!
//! ```ignore
//! use itertools::Itertools;
//! ```
//!
//! To enable the macros in this crate, use the `#[macro_use]` attribute:
//!
//! ```ignore
//! #[macro_use] extern crate itertools;
//! ```
//!
//! ## License
//! Dual-licensed to be compatible with the Rust project.
//!
//! Licensed under the Apache License, Version 2.0
//! http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
//! http://opensource.org/licenses/MIT, at your
//! option. This file may not be copied, modified, or distributed
//! except according to those terms.
//!
//!
#![doc(html_root_url="https://docs.rs/itertools/")]

extern crate either;

pub use either::Either;

use std::iter::{IntoIterator};
use std::fmt::Write;
use std::cmp::Ordering;
use std::fmt;
use std::hash::Hash;

#[macro_use]
mod impl_macros;

/// The concrete iterator types.
pub mod structs {
    pub use adaptors::{
        Dedup,
        Interleave,
        InterleaveShortest,
        Product,
        PutBack,
        PutBackN,
        Batching,
        Step,
        MapResults,
        Merge,
        MergeBy,
        MultiPeek,
        TakeWhileRef,
        WhileSome,
        Coalesce,
        TupleCombinations,
        Combinations,
        Unique,
        UniqueBy,
        Flatten,
    };
    pub use cons_tuples_impl::ConsTuples;
    pub use format::{Format, FormatWith};
    pub use groupbylazy::{IntoChunks, Chunk, Chunks, GroupBy, Group, Groups};
    pub use intersperse::Intersperse;
    pub use kmerge_impl::{KMerge, KMergeBy};
    pub use pad_tail::PadUsing;
    pub use peeking_take_while::PeekingTakeWhile;
    pub use rciter_impl::RcIter;
    pub use repeatn::RepeatN;
    pub use sources::{RepeatCall, Unfold, Iterate};
    pub use tee::Tee;
    pub use tuple_impl::{TupleBuffer, TupleWindows, Tuples};
    pub use with_position::WithPosition;
    pub use zip_eq_impl::ZipEq;
    pub use zip_longest::ZipLongest;
    pub use ziptuple::Zip;
}
pub use structs::*;
pub use cons_tuples_impl::cons_tuples;
pub use diff::diff_with;
pub use diff::Diff;
pub use kmerge_impl::{kmerge_by};
pub use minmax::MinMaxResult;
pub use peeking_take_while::PeekingNext;
pub use repeatn::repeat_n;
pub use sources::{repeat_call, unfold, iterate};
pub use with_position::Position;
pub use zip_longest::EitherOrBoth;
pub use ziptuple::multizip;
mod adaptors;
#[doc(hidden)]
pub mod free;
#[doc(inline)]
pub use free::*;
mod cons_tuples_impl;
mod diff;
mod format;
mod groupbylazy;
mod intersperse;
mod kmerge_impl;
mod minmax;
mod pad_tail;
mod peeking_take_while;
mod rciter_impl;
mod repeatn;
mod size_hint;
mod sources;
mod tee;
mod tuple_impl;
mod with_position;
mod zip_eq_impl;
mod zip_longest;
mod ziptuple;

#[macro_export]
/// Create an iterator over the “cartesian product” of iterators.
///
/// Iterator element type is like `(A, B, ..., E)` if formed
/// from iterators `(I, J, ..., M)` with element types `I::Item = A`, `J::Item = B`, etc.
///
/// ```
/// #[macro_use] extern crate itertools;
/// # fn main() {
/// // Iterate over the coordinates of a 4 x 4 x 4 grid
/// // from (0, 0, 0), (0, 0, 1), .., (0, 1, 0), (0, 1, 1), .. etc until (3, 3, 3)
/// for (i, j, k) in iproduct!(0..4, 0..4, 0..4) {
///    // ..
/// }
/// # }
/// ```
macro_rules! iproduct {
    (@flatten $I:expr,) => (
        $I
    );
    (@flatten $I:expr, $J:expr, $($K:expr,)*) => (
        iproduct!(@flatten $crate::cons_tuples(iproduct!($I, $J)), $($K,)*)
    );
    ($I:expr) => (
        (::std::iter::IntoIterator::into_iter($I))
    );
    ($I:expr, $J:expr) => (
        $crate::Itertools::cartesian_product(iproduct!($I), iproduct!($J))
    );
    ($I:expr, $J:expr, $($K:expr),+) => (
        iproduct!(@flatten iproduct!($I, $J), $($K,)+)
    );
}

#[macro_export]
/// Create an iterator running multiple iterators in lockstep.
///
/// The izip! iterator yields elements until any subiterator
/// returns `None`.
///
/// Iterator element type is like `(A, B, ..., E)` if formed
/// from iterators `(I, J, ..., M)` implementing `I: Iterator<A>`,
/// `J: Iterator<B>`, ..., `M: Iterator<E>`
///
/// ```
/// #[macro_use] extern crate itertools;
/// # fn main() {
///
/// // Iterate over three sequences side-by-side
/// let mut xs = [0, 0, 0];
/// let ys = [69, 107, 101];
///
/// for (i, a, b) in izip!(0..100, &mut xs, &ys) {
///    *a = i ^ *b;
/// }
///
/// assert_eq!(xs, [69, 106, 103]);
/// # }
/// ```
macro_rules! izip {
    ($I:expr) => (
        (::std::iter::IntoIterator::into_iter($I))
    );
    ($($I:expr),*) => (
        {
            $crate::multizip(($(izip!($I)),*))
        }
    );
}

/// The trait `Itertools`: extra iterator adaptors and methods for iterators.
///
/// This trait defines a number of methods. They are divided into two groups:
///
/// * *Adaptors* take an iterator and parameter as input, and return
/// a new iterator value. These are listed first in the trait. An example
/// of an adaptor is [`.interleave()`](#method.interleave)
///
/// * *Regular methods* are those that don't return iterators and instead
/// return a regular value of some other kind.
/// [`.next_tuple()`](#method.next_tuple) is an example and the first regular
/// method in the list.
pub trait Itertools : Iterator {
    // adaptors

    /// Alternate elements from two iterators until both
    /// run out.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// This iterator is *fused*.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (0..3).interleave(vec![7, 8]);
    /// itertools::assert_equal(it, vec![0, 7, 1, 8, 2]);
    /// ```
    fn interleave<J>(self, other: J) -> Interleave<Self, J::IntoIter>
        where J: IntoIterator<Item = Self::Item>,
              Self: Sized
    {
        interleave(self, other)
    }

    /// Alternate elements from two iterators until one of them runs out.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (0..5).interleave_shortest(vec![7, 8]);
    /// itertools::assert_equal(it, vec![0, 7, 1, 8, 2]);
    /// ```
    fn interleave_shortest<J>(self, other: J) -> InterleaveShortest<Self, J::IntoIter>
        where J: IntoIterator<Item = Self::Item>,
              Self: Sized
    {
        adaptors::interleave_shortest(self, other.into_iter())
    }

    /// An iterator adaptor to insert a particular value
    /// between each element of the adapted iterator.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// This iterator is *fused*.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// itertools::assert_equal((0..3).intersperse(8), vec![0, 8, 1, 8, 2]);
    /// ```
    fn intersperse(self, element: Self::Item) -> Intersperse<Self>
        where Self: Sized,
              Self::Item: Clone
    {
        intersperse::intersperse(self, element)
    }

    /// Create an iterator which iterates over both this and the specified
    /// iterator simultaneously, yielding pairs of two optional elements.
    ///
    /// This iterator is *fused*.
    ///
    /// When both iterators return `None`, all further invocations of `.next()`
    /// will return `None`.
    ///
    /// Iterator element type is
    /// [`EitherOrBoth<Self::Item, J::Item>`](enum.EitherOrBoth.html).
    ///
    /// ```rust
    /// use itertools::EitherOrBoth::{Both, Right};
    /// use itertools::Itertools;
    /// let it = (0..1).zip_longest(1..3);
    /// itertools::assert_equal(it, vec![Both(0, 1), Right(2)]);
    /// ```
    #[inline]
    fn zip_longest<J>(self, other: J) -> ZipLongest<Self, J::IntoIter>
        where J: IntoIterator,
              Self: Sized
    {
        zip_longest::zip_longest(self, other.into_iter())
    }

    /// Create an iterator which iterates over both this and the specified
    /// iterator simultaneously, yielding pairs of elements.
    ///
    /// **Panics** if the iterators reach an end and they are not of equal
    /// lengths.
    #[inline]
    fn zip_eq<J>(self, other: J) -> ZipEq<Self, J::IntoIter>
        where J: IntoIterator,
              Self: Sized
    {
        zip_eq(self, other)
    }

    /// A “meta iterator adaptor”. Its closure recives a reference to the
    /// iterator and may pick off as many elements as it likes, to produce the
    /// next iterator element.
    ///
    /// Iterator element type is `B`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // An adaptor that gathers elements up in pairs
    /// let pit = (0..4).batching(|mut it| {
    ///            match it.next() {
    ///                None => None,
    ///                Some(x) => match it.next() {
    ///                    None => None,
    ///                    Some(y) => Some((x, y)),
    ///                }
    ///            }
    ///        });
    ///
    /// itertools::assert_equal(pit, vec![(0, 1), (2, 3)]);
    /// ```
    ///
    fn batching<B, F>(self, f: F) -> Batching<Self, F>
        where F: FnMut(&mut Self) -> Option<B>,
              Self: Sized
    {
        adaptors::batching(self, f)
    }

    /// Return an *iterable* that can group iterator elements.
    /// Consecutive elements that map to the same key (“runs”), are assigned
    /// to the same group.
    ///
    /// `GroupBy` is the storage for the lazy grouping operation.
    ///
    /// If the groups are consumed in order, or if each group's iterator is
    /// dropped without keeping it around, then `GroupBy` uses no
    /// allocations.  It needs allocations only if several group iterators
    /// are alive at the same time.
    ///
    /// This type implements `IntoIterator` (it is **not** an iterator
    /// itself), because the group iterators need to borrow from this
    /// value. It should be stored in a local variable or temporary and
    /// iterated.
    ///
    /// Iterator element type is `(K, Group)`: the group's key and the
    /// group iterator.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // group data into runs of larger than zero or not.
    /// let data = vec![1, 3, -2, -2, 1, 0, 1, 2];
    /// // groups:     |---->|------>|--------->|
    ///
    /// // Note: The `&` is significant here, `GroupBy` is iterable
    /// // only by reference. You can also call `.into_iter()` explicitly.
    /// for (key, group) in &data.into_iter().group_by(|elt| *elt >= 0) {
    ///     // Check that the sum of each group is +/- 4.
    ///     assert_eq!(4, group.sum::<i32>().abs());
    /// }
    /// ```
    fn group_by<K, F>(self, key: F) -> GroupBy<K, Self, F>
        where Self: Sized,
              F: FnMut(&Self::Item) -> K,
    {
        groupbylazy::new(self, key)
    }

    ///
    #[deprecated(note = "renamed to .group_by()")]
    fn group_by_lazy<K, F>(self, key: F) -> GroupBy<K, Self, F>
        where Self: Sized,
              F: FnMut(&Self::Item) -> K,
    {
        self.group_by(key)
    }

    /// Return an *iterable* that can chunk the iterator.
    ///
    /// Yield subiterators (chunks) that each yield a fixed number elements,
    /// determined by `size`. The last chunk will be shorter if there aren't
    /// enough elements.
    ///
    /// `IntoChunks` is based on `GroupBy`: it is iterable (implements
    /// `IntoIterator`, **not** `Iterator`), and it only buffers if several
    /// chunk iterators are alive at the same time.
    ///
    /// Iterator element type is `Chunk`, each chunk's iterator.
    ///
    /// **Panics** if `size` is 0.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = vec![1, 1, 2, -2, 6, 0, 3, 1];
    /// //chunk size=3 |------->|-------->|--->|
    ///
    /// // Note: The `&` is significant here, `IntoChunks` is iterable
    /// // only by reference. You can also call `.into_iter()` explicitly.
    /// for chunk in &data.into_iter().chunks(3) {
    ///     // Check that the sum of each chunk is 4.
    ///     assert_eq!(4, chunk.sum());
    /// }
    /// ```
    fn chunks(self, size: usize) -> IntoChunks<Self>
        where Self: Sized,
    {
        assert!(size != 0);
        groupbylazy::new_chunks(self, size)
    }

    ///
    #[deprecated(note = "renamed to .chunks()")]
    fn chunks_lazy(self, size: usize) -> IntoChunks<Self>
        where Self: Sized,
    {
        self.chunks(size)
    }

    /// Return an iterator over all contiguous windows producing tuples of
    /// a specific size (up to 4).
    ///
    /// `tuple_windows` clones the iterator elements so that they can be
    /// part of successive windows, this makes it most suited for iterators
    /// of references and other values that are cheap to copy.
    ///
    /// ```
    /// use itertools::Itertools;
    /// let mut v = Vec::new();
    /// for (a, b) in (1..5).tuple_windows() {
    ///     v.push((a, b));
    /// }
    /// assert_eq!(v, vec![(1, 2), (2, 3), (3, 4)]);
    ///
    /// let mut it = (1..5).tuple_windows();
    /// assert_eq!(Some((1, 2, 3)), it.next());
    /// assert_eq!(Some((2, 3, 4)), it.next());
    /// assert_eq!(None, it.next());
    ///
    /// // this requires a type hint
    /// let it = (1..5).tuple_windows::<(_, _, _)>();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (2, 3, 4)]);
    ///
    /// // you can also specify the complete type
    /// use itertools::TupleWindows;
    /// use std::ops::Range;
    ///
    /// let it: TupleWindows<Range<u32>, (u32, u32, u32)> = (1..5).tuple_windows();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (2, 3, 4)]);
    /// ```
    fn tuple_windows<T>(self) -> TupleWindows<Self, T>
        where Self: Sized + Iterator<Item = T::Item>,
              T: tuple_impl::TupleCollect,
              T::Item: Clone
    {
        tuple_impl::tuple_windows(self)
    }

    /// Return an iterator that groups the items in tuples of a specific size
    /// (up to 4).
    ///
    /// See also the method [`.next_tuple()`](#method.next_tuple).
    ///
    /// ```
    /// use itertools::Itertools;
    /// let mut v = Vec::new();
    /// for (a, b) in (1..5).tuples() {
    ///     v.push((a, b));
    /// }
    /// assert_eq!(v, vec![(1, 2), (3, 4)]);
    ///
    /// let mut it = (1..7).tuples();
    /// assert_eq!(Some((1, 2, 3)), it.next());
    /// assert_eq!(Some((4, 5, 6)), it.next());
    /// assert_eq!(None, it.next());
    ///
    /// // this requires a type hint
    /// let it = (1..7).tuples::<(_, _, _)>();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (4, 5, 6)]);
    ///
    /// // you can also specify the complete type
    /// use itertools::Tuples;
    /// use std::ops::Range;
    ///
    /// let it: Tuples<Range<u32>, (u32, u32, u32)> = (1..7).tuples();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (4, 5, 6)]);
    /// ```
    ///
    /// See also [`Tuples::into_buffer`](structs/struct.Tuples.html#method.into_buffer).
    fn tuples<T>(self) -> Tuples<Self, T>
        where Self: Sized + Iterator<Item = T::Item>,
              T: tuple_impl::TupleCollect
    {
        tuple_impl::tuples(self)
    }

    /// Split into an iterator pair that both yield all elements from
    /// the original iterator.
    ///
    /// **Note:** If the iterator is clonable, prefer using that instead
    /// of using this method. It is likely to be more efficient.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    /// let xs = vec![0, 1, 2, 3];
    ///
    /// let (mut t1, t2) = xs.into_iter().tee();
    /// itertools::assert_equal(t1.next(), Some(0));
    /// itertools::assert_equal(t2, 0..4);
    /// itertools::assert_equal(t1, 1..4);
    /// ```
    fn tee(self) -> (Tee<Self>, Tee<Self>)
        where Self: Sized,
              Self::Item: Clone
    {
        tee::new(self)
    }

    /// Return an iterator adaptor that steps `n` elements in the base iterator
    /// for each iteration.
    ///
    /// The iterator steps by yielding the next element from the base iterator,
    /// then skipping forward `n - 1` elements.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// **Panics** if the step is 0.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (0..8).step(3);
    /// itertools::assert_equal(it, vec![0, 3, 6]);
    /// ```
    fn step(self, n: usize) -> Step<Self>
        where Self: Sized
    {
        adaptors::step(self, n)
    }

    /// Return an iterator adaptor that applies the provided closure
    /// to every `Result::Ok` value. `Result::Err` values are
    /// unchanged.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let input = vec![Ok(41), Err(false), Ok(11)];
    /// let it = input.into_iter().map_results(|i| i + 1);
    /// itertools::assert_equal(it, vec![Ok(42), Err(false), Ok(12)]);
    /// ```
    fn map_results<F, T, U, E>(self, f: F) -> MapResults<Self, F>
        where Self: Iterator<Item = Result<T, E>> + Sized,
              F: FnMut(T) -> U,
    {
        adaptors::map_results(self, f)
    }

    /// Return an iterator adaptor that merges the two base iterators in
    /// ascending order.  If both base iterators are sorted (ascending), the
    /// result is sorted.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let a = (0..11).step(3);
    /// let b = (0..11).step(5);
    /// let it = a.merge(b);
    /// itertools::assert_equal(it, vec![0, 0, 3, 5, 6, 9, 10]);
    /// ```
    fn merge<J>(self, other: J) -> Merge<Self, J::IntoIter>
        where Self: Sized,
              Self::Item: PartialOrd,
              J: IntoIterator<Item = Self::Item>
    {
        merge(self, other)
    }

    /// Return an iterator adaptor that merges the two base iterators in order.
    /// This is much like `.merge()` but allows for a custom ordering.
    ///
    /// This can be especially useful for sequences of tuples.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let a = (0..).zip("bc".chars());
    /// let b = (0..).zip("ad".chars());
    /// let it = a.merge_by(b, |x, y| x.1 <= y.1);
    /// itertools::assert_equal(it, vec![(0, 'a'), (0, 'b'), (1, 'c'), (1, 'd')]);
    /// ```

    fn merge_by<J, F>(self, other: J, is_first: F) -> MergeBy<Self, J::IntoIter, F>
        where Self: Sized,
              J: IntoIterator<Item = Self::Item>,
              F: FnMut(&Self::Item, &Self::Item) -> bool
    {
        adaptors::merge_by_new(self, other.into_iter(), is_first)
    }

    /// Return an iterator adaptor that flattens an iterator of iterators by
    /// merging them in ascending order.
    ///
    /// If all base iterators are sorted (ascending), the result is sorted.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let a = (0..6).step(3);
    /// let b = (1..6).step(3);
    /// let c = (2..6).step(3);
    /// let it = vec![a, b, c].into_iter().kmerge();
    /// itertools::assert_equal(it, vec![0, 1, 2, 3, 4, 5]);
    /// ```
    fn kmerge(self) -> KMerge<<<Self as Iterator>::Item as IntoIterator>::IntoIter>
        where Self: Sized,
              Self::Item: IntoIterator,
              <<Self as Iterator>::Item as IntoIterator>::Item: PartialOrd,
    {
        kmerge(self)
    }

    /// Return an iterator adaptor that flattens an iterator of iterators by
    /// merging them according to the given closure.
    ///
    /// The closure `first` is called with two elements *a*, *b* and should
    /// return `true` if *a* is ordered before *b*.
    ///
    /// If all base iterators are sorted according to `first`, the result is
    /// sorted.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let a = vec![-1f64, 2., 3., -5., 6., -7.];
    /// let b = vec![0., 2., -4.];
    /// let mut it = vec![a, b].into_iter().kmerge_by(|a, b| a.abs() < b.abs());
    /// assert_eq!(it.next(), Some(0.));
    /// assert_eq!(it.last(), Some(-7.));
    /// ```
    fn kmerge_by<F>(self, first: F)
        -> KMergeBy<<<Self as Iterator>::Item as IntoIterator>::IntoIter, F>
        where Self: Sized,
              Self::Item: IntoIterator,
              F: FnMut(&<<Self as Iterator>::Item as IntoIterator>::Item,
                       &<<Self as Iterator>::Item as IntoIterator>::Item) -> bool
    {
        kmerge_by(self, first)
    }

    /// Return an iterator adaptor that iterates over the cartesian product of
    /// the element sets of two iterators `self` and `J`.
    ///
    /// Iterator element type is `(Self::Item, J::Item)`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (0..2).cartesian_product("αβ".chars());
    /// itertools::assert_equal(it, vec![(0, 'α'), (0, 'β'), (1, 'α'), (1, 'β')]);
    /// ```
    fn cartesian_product<J>(self, other: J) -> Product<Self, J::IntoIter>
        where Self: Sized,
              Self::Item: Clone,
              J: IntoIterator,
              J::IntoIter: Clone
    {
        adaptors::cartesian_product(self, other.into_iter())
    }

    /// Return an iterator adaptor that uses the passed-in closure to
    /// optionally merge together consecutive elements.
    ///
    /// The closure `f` is passed two elements, `x`, `y` and may return either
    /// (1) `Ok(z)` to merge the two values or (2) `Err((x', y'))` to indicate
    /// they can't be merged. In (2), the value `x'` is emitted by the iterator.
    /// Coalesce continues with either `z` (1) or `y'` (2), and the next
    /// iterator element as the next pair of elements to merge.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// This iterator is *fused*.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // sum same-sign runs together
    /// let data = vec![-1., -2., -3., 3., 1., 0., -1.];
    /// itertools::assert_equal(data.into_iter().coalesce(|x, y|
    ///         if (x >= 0.) == (y >= 0.) {
    ///             Ok(x + y)
    ///         } else {
    ///             Err((x, y))
    ///         }),
    ///         vec![-6., 4., -1.]);
    /// ```
    fn coalesce<F>(self, f: F) -> Coalesce<Self, F>
        where Self: Sized,
              F: FnMut(Self::Item, Self::Item)
                       -> Result<Self::Item, (Self::Item, Self::Item)>
    {
        adaptors::coalesce(self, f)
    }

    /// Remove duplicates from sections of consecutive identical elements.
    /// If the iterator is sorted, all elements will be unique.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// This iterator is *fused*.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = vec![1., 1., 2., 3., 3., 2., 2.];
    /// itertools::assert_equal(data.into_iter().dedup(),
    ///                         vec![1., 2., 3., 2.]);
    /// ```
    fn dedup(self) -> Dedup<Self>
        where Self: Sized,
              Self::Item: PartialEq,
    {
        adaptors::dedup(self)
    }

    /// Return an iterator adaptor that filters out elements that have
    /// already been produced once during the iteration. Duplicates
    /// are detected using hash and equality.
    ///
    /// Clones of visited elements are stored in a hash set in the
    /// iterator.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = vec![10, 20, 30, 20, 40, 10, 50];
    /// itertools::assert_equal(data.into_iter().unique(),
    ///                         vec![10, 20, 30, 40, 50]);
    /// ```
    fn unique(self) -> Unique<Self>
        where Self: Sized,
              Self::Item: Clone + Eq + Hash
    {
        adaptors::unique(self)
    }

    /// Return an iterator adaptor that filters out elements that have
    /// already been produced once during the iteration.
    ///
    /// Duplicates are detected by comparing the key they map to
    /// with the keying function `f` by hash and equality.
    /// The keys are stored in a hash set in the iterator.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = vec!["a", "bb", "aa", "c", "ccc"];
    /// itertools::assert_equal(data.into_iter().unique_by(|s| s.len()),
    ///                         vec!["a", "bb", "ccc"]);
    /// ```
    fn unique_by<V, F>(self, f: F) -> UniqueBy<Self, V, F>
        where Self: Sized,
              V: Eq + Hash,
              F: FnMut(&Self::Item) -> V
    {
        adaptors::unique_by(self, f)
    }

    /// Return an iterator adaptor that borrows from this iterator and 
    /// takes items while the closure `accept` returns `true`.
    ///
    /// This adaptor can only be used on iterators that implement `PeekingNext`
    /// like `.peekable()`, `put_back` and a few other collection iterators.
    ///
    /// The last and rejected element (first `false`) is still available when
    /// `peeking_take_while` is done.
    ///
    ///
    /// See also [`.take_while_ref()`](#method.take_while_ref)
    /// which is a similar adaptor.
    fn peeking_take_while<F>(&mut self, accept: F) -> PeekingTakeWhile<Self, F>
        where Self: Sized + PeekingNext,
              F: FnMut(&Self::Item) -> bool,
    {
        peeking_take_while::peeking_take_while(self, accept)
    }

    /// Return an iterator adaptor that borrows from a `Clone`-able iterator
    /// to only pick off elements while the predicate `accept` returns `true`.
    ///
    /// It uses the `Clone` trait to restore the original iterator so that the
    /// last and rejected element (first `false`) is still available when
    /// `take_while_ref` is done.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let mut hexadecimals = "0123456789abcdef".chars();
    ///
    /// let decimals = hexadecimals.take_while_ref(|c| c.is_numeric())
    ///                            .collect::<String>();
    /// assert_eq!(decimals, "0123456789");
    /// assert_eq!(hexadecimals.next(), Some('a'));
    ///
    /// ```
    fn take_while_ref<F>(&mut self, accept: F) -> TakeWhileRef<Self, F>
        where Self: Clone,
              F: FnMut(&Self::Item) -> bool
    {
        adaptors::take_while_ref(self, accept)
    }

    /// Return an iterator adaptor that filters `Option<A>` iterator elements
    /// and produces `A`. Stops on the first `None` encountered.
    ///
    /// Iterator element type is `A`, the unwrapped element.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // List all hexadecimal digits
    /// itertools::assert_equal(
    ///     (0..).map(|i| std::char::from_digit(i, 16)).while_some(),
    ///     "0123456789abcdef".chars());
    ///
    /// ```
    fn while_some<A>(self) -> WhileSome<Self>
        where Self: Sized + Iterator<Item = Option<A>>
    {
        adaptors::while_some(self)
    }

    /// Return an iterator adaptor that iterates over the combinations of the
    /// elements from an iterator.
    ///
    /// Iterator element can be any homogeneous tuple of type `Self::Item` with
    /// size up to 4.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let mut v = Vec::new();
    /// for (a, b) in (1..5).tuple_combinations() {
    ///     v.push((a, b));
    /// }
    /// assert_eq!(v, vec![(1, 2), (1, 3), (1, 4), (2, 3), (2, 4), (3, 4)]);
    ///
    /// let mut it = (1..5).tuple_combinations();
    /// assert_eq!(Some((1, 2, 3)), it.next());
    /// assert_eq!(Some((1, 2, 4)), it.next());
    /// assert_eq!(Some((1, 3, 4)), it.next());
    /// assert_eq!(Some((2, 3, 4)), it.next());
    /// assert_eq!(None, it.next());
    ///
    /// // this requires a type hint
    /// let it = (1..5).tuple_combinations::<(_, _, _)>();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (1, 2, 4), (1, 3, 4), (2, 3, 4)]);
    ///
    /// // you can also specify the complete type
    /// use itertools::TupleCombinations;
    /// use std::ops::Range;
    ///
    /// let it: TupleCombinations<Range<u32>, (u32, u32, u32)> = (1..5).tuple_combinations();
    /// itertools::assert_equal(it, vec![(1, 2, 3), (1, 2, 4), (1, 3, 4), (2, 3, 4)]);
    /// ```
    fn tuple_combinations<T>(self) -> TupleCombinations<Self, T>
        where Self: Sized + Clone,
              Self::Item: Clone,
              T: adaptors::HasCombination<Self>,
    {
        adaptors::tuple_combinations(self)
    }

    /// Return an iterator adaptor that iterates over the `n`-length combinations of
    /// the elements from an iterator.
    ///
    /// Iterator element type is `Vec<Self::Item>`. The iterator produces a new Vec per iteration,
    /// and clones the iterator elements.
    ///
    /// **Panics** if `n` is zero.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (1..5).combinations(3);
    /// itertools::assert_equal(it, vec![
    ///     vec![1, 2, 3],
    ///     vec![1, 2, 4],
    ///     vec![1, 3, 4],
    ///     vec![2, 3, 4],
    ///     ]);
    /// ```
    fn combinations(self, n: usize) -> Combinations<Self>
        where Self: Sized,
              Self::Item: Clone
    {
        adaptors::combinations(self, n)
    }

    /// Return an iterator adaptor that pads the sequence to a minimum length of
    /// `min` by filling missing elements using a closure `f`.
    ///
    /// Iterator element type is `Self::Item`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let it = (0..5).pad_using(10, |i| 2*i);
    /// itertools::assert_equal(it, vec![0, 1, 2, 3, 4, 10, 12, 14, 16, 18]);
    ///
    /// let it = (0..10).pad_using(5, |i| 2*i);
    /// itertools::assert_equal(it, vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    ///
    /// let it = (0..5).pad_using(10, |i| 2*i).rev();
    /// itertools::assert_equal(it, vec![18, 16, 14, 12, 10, 4, 3, 2, 1, 0]);
    /// ```
    fn pad_using<F>(self, min: usize, f: F) -> PadUsing<Self, F>
        where Self: Sized,
              F: FnMut(usize) -> Self::Item
    {
        pad_tail::pad_using(self, min, f)
    }

    /// Unravel a nested iterator.
    ///
    /// This is a shortcut for `it.flat_map(|x| x)`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = vec![vec![1, 2, 3], vec![4, 5, 6]];
    /// let flattened = data.into_iter().flatten();
    ///
    /// itertools::assert_equal(flattened, vec![1, 2, 3, 4, 5, 6]);
    /// ```
    fn flatten(self) -> Flatten<Self, <<Self as Iterator>::Item as IntoIterator>::IntoIter>
        where Self: Sized,
              Self::Item: IntoIterator
    {
        adaptors::flatten(self)
    }

    /// Return an iterator adaptor that wraps each element in a `Position` to
    /// ease special-case handling of the first or last elements.
    ///
    /// Iterator element type is
    /// [`Position<Self::Item>`](enum.Position.html)
    ///
    /// ```
    /// use itertools::{Itertools, Position};
    ///
    /// let it = (0..4).with_position();
    /// itertools::assert_equal(it,
    ///                         vec![Position::First(0),
    ///                              Position::Middle(1),
    ///                              Position::Middle(2),
    ///                              Position::Last(3)]);
    ///
    /// let it = (0..1).with_position();
    /// itertools::assert_equal(it, vec![Position::Only(0)]);
    /// ```
    fn with_position(self) -> WithPosition<Self>
        where Self: Sized,
    {
        with_position::with_position(self)
    }

    // non-adaptor methods
    /// Advances the iterator and returns the next items grouped in a tuple of
    /// a specific size (up to 4).
    ///
    /// If there are enough elements to be grouped in a tuple, then the tuple is
    /// returned inside `Some`, otherwise `None` is returned.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let mut iter = 1..5;
    ///
    /// assert_eq!(Some((1, 2)), iter.next_tuple());
    /// ```
    fn next_tuple<T>(&mut self) -> Option<T>
        where Self: Sized + Iterator<Item = T::Item>,
              T: tuple_impl::TupleCollect
    {
        T::collect_from_iter_no_buf(self)
    }


    /// Find the position and value of the first element satisfying a predicate.
    ///
    /// The iterator is not advanced past the first element found.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let text = "Hα";
    /// assert_eq!(text.chars().find_position(|ch| ch.is_lowercase()), Some((1, 'α')));
    /// ```
    fn find_position<P>(&mut self, mut pred: P) -> Option<(usize, Self::Item)>
        where P: FnMut(&Self::Item) -> bool
    {
        let mut index = 0usize;
        for elt in self {
            if pred(&elt) {
                return Some((index, elt));
            }
            index += 1;
        }
        None
    }

    /// Consume the first `n` elements from the iterator eagerly,
    /// and return the same iterator again.
    ///
    /// It works similarly to *.skip(* `n` *)* except it is eager and
    /// preserves the iterator type.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let mut iter = "αβγ".chars().dropping(2);
    /// itertools::assert_equal(iter, "γ".chars());
    /// ```
    ///
    /// *Fusing notes: if the iterator is exhausted by dropping,
    /// the result of calling `.next()` again depends on the iterator implementation.*
    fn dropping(mut self, n: usize) -> Self
        where Self: Sized
    {
        if n > 0 {
            self.nth(n - 1);
        }
        self
    }

    /// Consume the last `n` elements from the iterator eagerly,
    /// and return the same iterator again.
    ///
    /// This is only possible on double ended iterators. `n` may be
    /// larger than the number of elements.
    ///
    /// Note: This method is eager, dropping the back elements immediately and
    /// preserves the iterator type.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let init = vec![0, 3, 6, 9].into_iter().dropping_back(1);
    /// itertools::assert_equal(init, vec![0, 3, 6]);
    /// ```
    fn dropping_back(mut self, n: usize) -> Self
        where Self: Sized,
              Self: DoubleEndedIterator
    {
        if n > 0 {
            (&mut self).rev().nth(n - 1);
        }
        self
    }

    /// Run the closure `f` eagerly on each element of the iterator.
    ///
    /// Consumes the iterator until its end.
    ///
    /// ```
    /// use std::sync::mpsc::channel;
    /// use itertools::Itertools;
    ///
    /// let (tx, rx) = channel();
    ///
    /// // use .foreach() to apply a function to each value -- sending it
    /// (0..5).map(|x| x * 2 + 1).foreach(|x| { tx.send(x).unwrap(); } );
    ///
    /// drop(tx);
    ///
    /// itertools::assert_equal(rx.iter(), vec![1, 3, 5, 7, 9]);
    /// ```
    fn foreach<F>(&mut self, mut f: F)
        where F: FnMut(Self::Item)
    {
        // FIXME: This use of fold doesn't actually do any iterator
        // specific traversal (fold requries `self`)
        self.fold((), move |(), element| f(element))
    }

    /// `.collect_vec()` is simply a type specialization of `.collect()`,
    /// for convenience.
    fn collect_vec(self) -> Vec<Self::Item>
        where Self: Sized
    {
        self.collect()
    }

    /// Assign to each reference in `self` from the `from` iterator,
    /// stopping at the shortest of the two iterators.
    ///
    /// The `from` iterator is queried for its next element before the `self`
    /// iterator, and if either is exhausted the method is done.
    ///
    /// Return the number of elements written.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let mut xs = [0; 4];
    /// xs.iter_mut().set_from(1..);
    /// assert_eq!(xs, [1, 2, 3, 4]);
    /// ```
    #[inline]
    fn set_from<'a, A: 'a, J>(&mut self, from: J) -> usize
        where Self: Iterator<Item = &'a mut A>,
              J: IntoIterator<Item = A>
    {
        let mut count = 0;
        for elt in from {
            match self.next() {
                None => break,
                Some(ptr) => *ptr = elt,
            }
            count += 1;
        }
        count
    }

    /// Combine all iterator elements into one String, seperated by `sep`.
    ///
    /// Use the `Display` implementation of each element.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// assert_eq!(["a", "b", "c"].iter().join(", "), "a, b, c");
    /// assert_eq!([1, 2, 3].iter().join(", "), "1, 2, 3");
    /// ```
    fn join(&mut self, sep: &str) -> String
        where Self::Item: std::fmt::Display
    {
        match self.next() {
            None => String::new(),
            Some(first_elt) => {
                // estimate lower bound of capacity needed
                let (lower, _) = self.size_hint();
                let mut result = String::with_capacity(sep.len() * lower);
                write!(&mut result, "{}", first_elt).unwrap();
                for elt in self {
                    result.push_str(sep);
                    write!(&mut result, "{}", elt).unwrap();
                }
                result
            }
        }
    }

    /// Format all iterator elements, separated by `sep`.
    ///
    /// All elements are formatted (any formatting trait)
    /// with `sep` inserted between each element.
    ///
    /// **Panics** if the formatter helper is formatted more than once.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = [1.1, 2.71828, -3.];
    /// assert_eq!(
    ///     format!("{:.2}", data.iter().format(", ")),
    ///            "1.10, 2.72, -3.00");
    /// ```
    fn format(self, sep: &str) -> Format<Self>
        where Self: Sized,
    {
        format::new_format_default(self, sep)
    }

    ///
    #[deprecated(note = "renamed to .format()")]
    fn format_default(self, sep: &str) -> Format<Self>
        where Self: Sized,
    {
        format::new_format_default(self, sep)
    }

    /// Format all iterator elements, separated by `sep`.
    ///
    /// This is a customizable version of `.format()`.
    ///
    /// The supplied closure `format` is called once per iterator element,
    /// with two arguments: the element and a callback that takes a
    /// `&Display` value, i.e. any reference to type that implements `Display`.
    ///
    /// Using `&format_args!(...)` is the most versatile way to apply custom
    /// element formatting. The callback can be called multiple times if needed.
    ///
    /// **Panics** if the formatter helper is formatted more than once.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// let data = [1.1, 2.71828, -3.];
    /// let data_formatter = data.iter().format_with(", ", |elt, f| f(&format_args!("{:.2}", elt)));
    /// assert_eq!(format!("{}", data_formatter),
    ///            "1.10, 2.72, -3.00");
    ///
    /// // .format_with() is recursively composable
    /// let matrix = [[1., 2., 3.],
    ///               [4., 5., 6.]];
    /// let matrix_formatter = matrix.iter().format_with("\n", |row, f| {
    ///                                 f(&row.iter().format_with(", ", |elt, g| g(&elt)))
    ///                              });
    /// assert_eq!(format!("{}", matrix_formatter),
    ///            "1, 2, 3\n4, 5, 6");
    ///
    ///
    /// ```
    fn format_with<F>(self, sep: &str, format: F) -> FormatWith<Self, F>
        where Self: Sized,
              F: FnMut(Self::Item, &mut FnMut(&fmt::Display) -> fmt::Result) -> fmt::Result,
    {
        format::new_format(self, sep, format)
    }

    /// Fold `Result` values from an iterator.
    ///
    /// Only `Ok` values are folded. If no error is encountered, the folded
    /// value is returned inside `Ok`. Otherwise, the operation terminates
    /// and returns the first `Err` value it encounters. No iterator elements are
    /// consumed after the first error.
    ///
    /// The first accumulator value is the `start` parameter.
    /// Each iteration passes the accumulator value and the next value inside `Ok`
    /// to the fold function `f` and its return value becomes the new accumulator value.
    ///
    /// For example the sequence *Ok(1), Ok(2), Ok(3)* will result in a
    /// computation like this:
    ///
    /// ```ignore
    /// let mut accum = start;
    /// accum = f(accum, 1);
    /// accum = f(accum, 2);
    /// accum = f(accum, 3);
    /// ```
    ///
    /// With a `start` value of 0 and an addition as folding function,
    /// this effetively results in *((0 + 1) + 2) + 3*
    ///
    /// ```
    /// use std::ops::Add;
    /// use itertools::Itertools;
    ///
    /// let values = [1, 2, -2, -1, 2, 1];
    /// assert_eq!(
    ///     values.iter()
    ///           .map(Ok::<_, ()>)
    ///           .fold_results(0, Add::add),
    ///     Ok(3)
    /// );
    /// assert!(
    ///     values.iter()
    ///           .map(|&x| if x >= 0 { Ok(x) } else { Err("Negative number") })
    ///           .fold_results(0, Add::add)
    ///           .is_err()
    /// );
    /// ```
    fn fold_results<A, E, B, F>(&mut self, mut start: B, mut f: F) -> Result<B, E>
        where Self: Iterator<Item = Result<A, E>>,
              F: FnMut(B, A) -> B
    {
        for elt in self {
            match elt {
                Ok(v) => start = f(start, v),
                Err(u) => return Err(u),
            }
        }
        Ok(start)
    }

    /// Fold `Option` values from an iterator.
    ///
    /// Only `Some` values are folded. If no `None` is encountered, the folded
    /// value is returned inside `Some`. Otherwise, the operation terminates
    /// and returns `None`. No iterator elements are consumed after the `None`.
    ///
    /// This is the `Option` equivalent to `fold_results`.
    ///
    /// ```
    /// use std::ops::Add;
    /// use itertools::Itertools;
    ///
    /// let mut values = vec![Some(1), Some(2), Some(-2)].into_iter();
    /// assert_eq!(values.fold_options(5, Add::add), Some(5 + 1 + 2 - 2));
    ///
    /// let mut more_values = vec![Some(2), None, Some(0)].into_iter();
    /// assert!(more_values.fold_options(0, Add::add).is_none());
    /// assert_eq!(more_values.next().unwrap(), Some(0));
    /// ```
    fn fold_options<A, B, F>(&mut self, mut start: B, mut f: F) -> Option<B>
        where Self: Iterator<Item = Option<A>>,
              F: FnMut(B, A) -> B
    {
        for elt in self {
            match elt {
                Some(v) => start = f(start, v),
                None => return None,
            }
        }
        Some(start)
    }

    /// Accumulator of the elements in the iterator.
    ///
    /// Like `.fold()`, without a base case. If the iterator is
    /// empty, return `None`. With just one element, return it.
    /// Otherwise elements are accumulated in sequence using the closure `f`.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// assert_eq!((0..10).fold1(|x, y| x + y).unwrap_or(0), 45);
    /// assert_eq!((0..0).fold1(|x, y| x * y), None);
    /// ```
    fn fold1<F>(&mut self, mut f: F) -> Option<Self::Item>
        where F: FnMut(Self::Item, Self::Item) -> Self::Item
    {
        match self.next() {
            None => None,
            Some(mut x) => {
                for y in self {
                    x = f(x, y);
                }
                Some(x)
            }
        }
    }

    /// An iterator method that applies a function, producing a single, final value.
    ///
    /// `fold_while()` is basically equivalent to `fold()` but with additional support for
    /// early exit via short-circuiting.
    ///
    /// ```
    /// use itertools::Itertools;
    /// use itertools::FoldWhile::{Continue, Done};
    ///
    /// let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    ///
    /// let mut result = 0;
    ///
    /// // for loop:
    /// for i in &numbers {
    ///     if *i > 5 {
    ///         break;
    ///     }
    ///     result = result + i;
    /// }
    ///
    /// // fold:
    /// let result2 = numbers.iter().fold(0, |acc, x| {
    ///     if *x > 5 { acc } else { acc + x }
    /// });
    ///
    /// // fold_while:
    /// let result3 = numbers.iter().fold_while(0, |acc, x| {
    ///     if *x > 5 { Done(acc) } else { Continue(acc + x) }
    /// });
    ///
    /// // they're the same
    /// assert_eq!(result, result2);
    /// assert_eq!(result2, result3);
    /// ```
    ///
    /// The big difference between the computations of `result2` and `result3` is that while
    /// `fold()` called the provided closure for every item of the callee iterator,
    /// `fold_while()` actually stopped iterating as soon as it encountered `Fold::Done(_)`.
    fn fold_while<B, F>(self, init: B, mut f: F) -> B
        where Self: Sized,
              F: FnMut(B, Self::Item) -> FoldWhile<B>
    {
        let mut accum = init;
        for item in self {
            match f(accum, item) {
                FoldWhile::Continue(res) => {
                    accum = res;
                }
                FoldWhile::Done(res) => {
                    accum = res;
                    break;
                }
            }
        }
        accum
    }

    /// Collect all iterator elements into a sorted vector in ascending order.
    ///
    /// **Note:** This consumes the entire iterator, uses the
    /// `slice::sort_by()` method and returns the sorted vector.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // sort the letters of the text in ascending order
    /// let text = "bdacfe";
    /// itertools::assert_equal(text.chars().sorted(),
    ///                         "abcdef".chars());
    /// ```
    fn sorted(self) -> Vec<Self::Item>
        where Self: Sized,
              Self::Item: Ord
    {
        self.sorted_by(Ord::cmp)
    }

    /// Collect all iterator elements into a sorted vector.
    ///
    /// **Note:** This consumes the entire iterator, uses the
    /// `slice::sort_by()` method and returns the sorted vector.
    ///
    /// ```
    /// use itertools::Itertools;
    ///
    /// // sort people in descending order by age
    /// let people = vec![("Jane", 20), ("John", 18), ("Jill", 30), ("Jack", 27)];
    ///
    /// let oldest_people_first = people
    ///     .into_iter()
    ///     .sorted_by(|a, b| Ord::cmp(&b.1, &a.1))
    ///     .into_iter()
    ///     .map(|(person, _age)| person);
    ///
    /// itertools::assert_equal(oldest_people_first,
    ///                         vec!["Jill", "Jack", "Jane", "John"]);
    /// ```
    fn sorted_by<F>(self, cmp: F) -> Vec<Self::Item>
        where Self: Sized,
              F: FnMut(&Self::Item, &Self::Item) -> Ordering,
    {
        let mut v: Vec<Self::Item> = self.collect();

        v.sort_by(cmp);
        v
    }

    /// Collect all iterator elements into one of two
    /// partitions. Unlike `Iterator::partition`, each partition may
    /// have a distinct type.
    ///
    /// ```
    /// use itertools::{Itertools, Either};
    ///
    /// let successes_and_failures = vec![Ok(1), Err(false), Err(true), Ok(2)];
    ///
    /// let (successes, failures): (Vec<_>, Vec<_>) = successes_and_failures
    ///     .into_iter()
    ///     .partition_map(|r| {
    ///         match r {
    ///             Ok(v) => Either::Left(v),
    ///             Err(v) => Either::Right(v),
    ///         }
    ///     });
    ///
    /// assert_eq!(successes, [1, 2]);
    /// assert_eq!(failures, [false, true]);
    /// ```
    fn partition_map<A, B, F, L, R>(self, predicate: F) -> (A, B)
        where Self: Sized,
              F: Fn(Self::Item) -> Either<L, R>,
              A: Default + Extend<L>,
              B: Default + Extend<R>,
    {
        let mut left = A::default();
        let mut right = B::default();

        for val in self {
            match predicate(val) {
                Either::Left(v) => left.extend(Some(v)),
                Either::Right(v) => right.extend(Some(v)),
            }
        }

        (left, right)
    }

    /// Return the minimum and maximum elements in the iterator.
    ///
    /// The return type `MinMaxResult` is an enum of three variants:
    ///
    /// - `NoElements` if the iterator is empty.
    /// - `OneElement(x)` if the iterator has exactly one element.
    /// - `MinMax(x, y)` is returned otherwise, where `x <= y`. Two
    ///    values are equal if and only if there is more than one
    ///    element in the iterator and all elements are equal.
    ///
    /// On an iterator of length `n`, `minmax` does `1.5 * n` comparisons,
    /// and so is faster than calling `min` and `max` separately which does
    /// `2 * n` comparisons.
    ///
    /// # Examples
    ///
    /// ```
    /// use itertools::Itertools;
    /// use itertools::MinMaxResult::{NoElements, OneElement, MinMax};
    ///
    /// let a: [i32; 0] = [];
    /// assert_eq!(a.iter().minmax(), NoElements);
    ///
    /// let a = [1];
    /// assert_eq!(a.iter().minmax(), OneElement(&1));
    ///
    /// let a = [1, 2, 3, 4, 5];
    /// assert_eq!(a.iter().minmax(), MinMax(&1, &5));
    ///
    /// let a = [1, 1, 1, 1];
    /// assert_eq!(a.iter().minmax(), MinMax(&1, &1));
    /// ```
    ///
    /// The elements can be floats but no particular result is guaranteed
    /// if an element is NaN.
    fn minmax(self) -> MinMaxResult<Self::Item>
        where Self: Sized, Self::Item: PartialOrd
    {
        minmax::minmax_impl(self, |_| (), |x, y, _, _| x < y)
    }

    /// Return the minimum and maximum element of an iterator, as determined by
    /// the specified function.
    ///
    /// The return value is a variant of `MinMaxResult` like for `minmax()`.
    ///
    /// For the minimum, the first minimal element is returned.  For the maximum,
    /// the last maximal element wins.  This matches the behavior of the standard
    /// `Iterator::min()` and `Iterator::max()` methods.
    ///
    /// The keys can be floats but no particular result is guaranteed
    /// if a key is NaN.
    fn minmax_by_key<K, F>(self, key: F) -> MinMaxResult<Self::Item>
        where Self: Sized, K: PartialOrd, F: FnMut(&Self::Item) -> K
    {
        minmax::minmax_impl(self, key, |_, _, xk, yk| xk < yk)
    }

    /// Return the minimum and maximum element of an iterator, as determined by
    /// the specified comparison function.
    ///
    /// The return value is a variant of `MinMaxResult` like for `minmax()`.
    ///
    /// For the minimum, the first minimal element is returned.  For the maximum,
    /// the last maximal element wins.  This matches the behavior of the standard
    /// `Iterator::min()` and `Iterator::max()` methods.
    fn minmax_by<F>(self, mut compare: F) -> MinMaxResult<Self::Item>
        where Self: Sized, F: FnMut(&Self::Item, &Self::Item) -> Ordering
    {
        minmax::minmax_impl(
            self,
            |_| (),
            |x, y, _, _| Ordering::Less == compare(x, y)
        )
    }
}

impl<T: ?Sized> Itertools for T where T: Iterator { }

/// Return `true` if both iterators produce equal sequences
/// (elements pairwise equal and sequences of the same length),
/// `false` otherwise.
///
/// **Note:** the standard library method `Iterator::eq` now provides
/// the same functionality.
///
/// ```
/// assert!(itertools::equal(vec![1, 2, 3], 1..4));
/// assert!(!itertools::equal(&[0, 0], &[0, 0, 0]));
/// ```
pub fn equal<I, J>(a: I, b: J) -> bool
    where I: IntoIterator,
          J: IntoIterator,
          I::Item: PartialEq<J::Item>
{
    let mut ia = a.into_iter();
    let mut ib = b.into_iter();
    loop {
        match ia.next() {
            Some(ref x) => match ib.next() {
                Some(ref y) => if x != y { return false; },
                None => return false,
            },
            None => return ib.next().is_none()
        }
    }
}

/// Assert that two iterators produce equal sequences, with the same
/// semantics as *equal(a, b)*.
///
/// **Panics** on assertion failure with a message that shows the
/// two iteration elements.
///
/// ```ignore
/// assert_equal("exceed".split('c'), "excess".split('c'));
/// // ^PANIC: panicked at 'Failed assertion Some("eed") == Some("ess") for iteration 1',
/// ```
pub fn assert_equal<I, J>(a: I, b: J)
    where I: IntoIterator,
          J: IntoIterator,
          I::Item: fmt::Debug + PartialEq<J::Item>,
          J::Item: fmt::Debug,
{
    let mut ia = a.into_iter();
    let mut ib = b.into_iter();
    let mut i = 0;
    loop {
        match (ia.next(), ib.next()) {
            (None, None) => return,
            (a, b) => {
                let equal = match (&a, &b) {
                    (&Some(ref a), &Some(ref b)) => a == b,
                    _ => false,
                };
                assert!(equal, "Failed assertion {a:?} == {b:?} for iteration {i}",
                        i=i, a=a, b=b);
                i += 1;
            }
        }
    }
}

/// Partition a sequence using predicate `pred` so that elements
/// that map to `true` are placed before elements which map to `false`.
///
/// The order within the partitions is arbitrary.
///
/// Return the index of the split point.
///
/// ```
/// use itertools::partition;
///
/// # // use repeated numbers to not promise any ordering
/// let mut data = [7, 1, 1, 7, 1, 1, 7];
/// let split_index = partition(&mut data, |elt| *elt >= 3);
///
/// assert_eq!(data, [7, 7, 7, 1, 1, 1, 1]);
/// assert_eq!(split_index, 3);
/// ```
pub fn partition<'a, A: 'a, I, F>(iter: I, mut pred: F) -> usize
    where I: IntoIterator<Item = &'a mut A>,
          I::IntoIter: DoubleEndedIterator,
          F: FnMut(&A) -> bool
{
    let mut split_index = 0;
    let mut iter = iter.into_iter();
    'main: while let Some(front) = iter.next() {
        if !pred(front) {
            loop {
                match iter.next_back() {
                    Some(back) => if pred(back) {
                        std::mem::swap(front, back);
                        break;
                    },
                    None => break 'main,
                }
            }
        }
        split_index += 1;
    }
    split_index
}

/// An enum used for controlling the execution of `.fold_while()`.
///
/// See [`.fold_while()`](trait.Itertools.html#method.fold_while) for more information.
pub enum FoldWhile<T> {
    /// Continue folding with this value
    Continue(T),
    /// Fold is complete and will return this value
    Done(T),
}

