
use size_hint;
use Itertools;

use std::mem::replace;

macro_rules! clone_fields {
    ($name:ident, $base:expr, $($field:ident),+) => (
        $name {
            $(
                $field : $base . $field .clone()
            ),*
        }
    );
}

/// Head element and Tail iterator pair
///
/// `PartialEq`, `Eq`, `PartialOrd` and `Ord` are implemented by comparing sequences based on
/// first items (which are guaranteed to exist).
///
/// The meanings of `PartialOrd` and `Ord` are reversed so as to turn the heap used in
/// `KMerge` into a min-heap.
struct HeadTail<I>
    where I: Iterator
{
    head: I::Item,
    tail: I,
}

impl<I> HeadTail<I>
    where I: Iterator
{
    /// Constructs a `HeadTail` from an `Iterator`. Returns `None` if the `Iterator` is empty.
    fn new(mut it: I) -> Option<HeadTail<I>> {
        let head = it.next();
        head.map(|h| {
            HeadTail {
                head: h,
                tail: it,
            }
        })
    }

    /// Get the next element and update `head`, returning the old head in `Some`.
    ///
    /// Returns `None` when the tail is exhausted (only `head` then remains).
    fn next(&mut self) -> Option<I::Item> {
        if let Some(next) = self.tail.next() {
            Some(replace(&mut self.head, next))
        } else {
            None
        }
    }

    /// Hints at the size of the sequence, same as the `Iterator` method.
    fn size_hint(&self) -> (usize, Option<usize>) {
        size_hint::add_scalar(self.tail.size_hint(), 1)
    }
}

impl<I> Clone for HeadTail<I>
    where I: Iterator + Clone,
          I::Item: Clone
{
    fn clone(&self) -> Self {
        clone_fields!(HeadTail, self, head, tail)
    }
}

/// Make `data` a heap (min-heap w.r.t the sorting).
fn heapify<T, S>(data: &mut [T], mut less_than: S)
    where S: FnMut(&T, &T) -> bool
{
    for i in (0..data.len() / 2).rev() {
        sift_down(data, i, &mut less_than);
    }
}

/// Sift down element at `index` (`heap` is a min-heap wrt the ordering)
fn sift_down<T, S>(heap: &mut [T], index: usize, mut less_than: S)
    where S: FnMut(&T, &T) -> bool
{
    debug_assert!(index <= heap.len());
    let mut pos = index;
    let mut child = 2 * pos + 1;
    // the `pos` conditional is to avoid a bounds check
    while pos < heap.len() && child < heap.len() {
        let right = child + 1;

        // pick the smaller of the two children
        if right < heap.len() && less_than(&heap[right], &heap[child]) {
            child = right;
        }

        // sift down is done if we are already in order
        if !less_than(&heap[child], &heap[pos]) {
            return;
        }
        heap.swap(pos, child);
        pos = child;
        child = 2 * pos + 1;
    }
}

/// An iterator adaptor that merges an abitrary number of base iterators in ascending order.
/// If all base iterators are sorted (ascending), the result is sorted.
///
/// Iterator element type is `I::Item`.
///
/// See [`.kmerge()`](../trait.Itertools.html#method.kmerge) for more information.
pub struct KMerge<I>
    where I: Iterator
{
    heap: Vec<HeadTail<I>>,
}

/// Create an iterator that merges elements of the contained iterators using
/// the ordering function.
///
/// Equivalent to `iterable.into_iter().kmerge()`.
///
/// ```
/// use itertools::kmerge;
///
/// for elt in kmerge(vec![vec![0, 2, 4], vec![1, 3, 5], vec![6, 7]]) {
///     /* loop body */
/// }
/// ```
pub fn kmerge<I>(iterable: I) -> KMerge<<I::Item as IntoIterator>::IntoIter>
    where I: IntoIterator,
          I::Item: IntoIterator,
          <<I as IntoIterator>::Item as IntoIterator>::Item: PartialOrd
{
    let iter = iterable.into_iter();
    let (lower, _) = iter.size_hint();
    let mut heap = Vec::with_capacity(lower);
    heap.extend(iter.filter_map(|it| HeadTail::new(it.into_iter())));
    heapify(&mut heap, |a, b| a.head < b.head);
    KMerge { heap: heap }
}

impl<I> Clone for KMerge<I>
    where I: Iterator + Clone,
          I::Item: Clone
{
    fn clone(&self) -> KMerge<I> {
        clone_fields!(KMerge, self, heap)
    }
}

impl<I> Iterator for KMerge<I>
    where I: Iterator,
          I::Item: PartialOrd
{
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        if self.heap.is_empty() {
            return None;
        }
        let result = if let Some(next) = self.heap[0].next() {
            next
        } else {
            self.heap.swap_remove(0).head
        };
        sift_down(&mut self.heap, 0, |a, b| a.head < b.head);
        Some(result)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.heap.iter()
                 .map(|i| i.size_hint())
                 .fold1(size_hint::add)
                 .unwrap_or((0, Some(0)))
    }
}

/// An iterator adaptor that merges an abitrary number of base iterators
/// according to an ordering function.
///
/// Iterator element type is `I::Item`.
///
/// See [`.kmerge_by()`](../trait.Itertools.html#method.kmerge_by) for more
/// information.
pub struct KMergeBy<I, F>
    where I: Iterator,
{
    heap: Vec<HeadTail<I>>,
    less_than: F,
}

/// Create an iterator that merges elements of the contained iterators.
///
/// Equivalent to `iterable.into_iter().kmerge_by(less_than)`.
pub fn kmerge_by<I, F>(iterable: I, mut less_than: F)
    -> KMergeBy<<I::Item as IntoIterator>::IntoIter, F>
    where I: IntoIterator,
          I::Item: IntoIterator,
          F: FnMut(&<<I as IntoIterator>::Item as IntoIterator>::Item,
                   &<<I as IntoIterator>::Item as IntoIterator>::Item) -> bool
{
    let iter = iterable.into_iter();
    let (lower, _) = iter.size_hint();
    let mut heap: Vec<_> = Vec::with_capacity(lower);
    heap.extend(iter.filter_map(|it| HeadTail::new(it.into_iter())));
    heapify(&mut heap, |a, b| less_than(&a.head, &b.head));
    KMergeBy { heap: heap, less_than: less_than }
}


impl<I, F> Iterator for KMergeBy<I, F>
    where I: Iterator,
          F: FnMut(&I::Item, &I::Item) -> bool
{
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        if self.heap.is_empty() {
            return None;
        }
        let result = if let Some(next) = self.heap[0].next() {
            next
        } else {
            self.heap.swap_remove(0).head
        };
        let less_than = &mut self.less_than;
        sift_down(&mut self.heap, 0, |a, b| less_than(&a.head, &b.head));
        Some(result)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.heap.iter()
                 .map(|i| i.size_hint())
                 .fold1(size_hint::add)
                 .unwrap_or((0, Some(0)))
    }
}
