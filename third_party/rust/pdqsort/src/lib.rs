//! Pattern-defeating quicksort.
//!
//! This sort is significantly faster than the standard sort in Rust. In particular, it sorts
//! random arrays of integers approximately 40% faster. The key drawback is that it is an unstable
//! sort (i.e. may reorder equal elements). However, in most cases stability doesn't matter anyway.
//!
//! The algorithm was designed by Orson Peters and first published at:
//! https://github.com/orlp/pdqsort
//!
//! Quoting it's designer: "Pattern-defeating quicksort (pdqsort) is a novel sorting algorithm
//! that combines the fast average case of randomized quicksort with the fast worst case of
//! heapsort, while achieving linear time on inputs with certain patterns. pdqsort is an extension
//! and improvement of David Musser's introsort."
//!
//! # Properties
//!
//! - Best-case running time is `O(n)`.
//! - Worst-case running time is `O(n log n)`.
//! - Unstable, i.e. may reorder equal elements.
//! - Does not allocate additional memory.
//! - Uses `#![no_std]`.
//!
//! # Examples
//!
//! ```
//! extern crate pdqsort;
//!
//! let mut v = [-5i32, 4, 1, -3, 2];
//!
//! pdqsort::sort(&mut v);
//! assert!(v == [-5, -3, 1, 2, 4]);
//!
//! pdqsort::sort_by(&mut v, |a, b| b.cmp(a));
//! assert!(v == [4, 2, 1, -3, -5]);
//!
//! pdqsort::sort_by_key(&mut v, |k| k.abs());
//! assert!(v == [1, 2, -3, 4, -5]);
//! ```

#![no_std]

use core::cmp::{self, Ordering};
use core::mem;
use core::ptr;

/// On drop, takes the value out of `Option` and writes it into `dest`.
///
/// This allows us to safely read the pivot into a stack-allocated variable for efficiency, and
/// write it back into the slice after partitioning. This way we ensure that the write happens
/// even if `is_less` panics in the meantime.
struct WriteOnDrop<T> {
    value: Option<T>,
    dest: *mut T,
}

impl<T> Drop for WriteOnDrop<T> {
    fn drop(&mut self) {
        unsafe {
            ptr::write(self.dest, self.value.take().unwrap());
        }
    }
}

/// Inserts `v[0]` into pre-sorted sequence `v[1..]` so that whole `v[..]` becomes sorted.
///
/// This is the integral subroutine of insertion sort.
fn insert_head<T, F>(v: &mut [T], is_less: &mut F)
    where F: FnMut(&T, &T) -> bool
{
    if v.len() >= 2 && is_less(&v[1], &v[0]) {
        unsafe {
            // There are three ways to implement insertion here:
            //
            // 1. Swap adjacent elements until the first one gets to its final destination.
            //    However, this way we copy data around more than is necessary. If elements are big
            //    structures (costly to copy), this method will be slow.
            //
            // 2. Iterate until the right place for the first element is found. Then shift the
            //    elements succeeding it to make room for it and finally place it into the
            //    remaining hole. This is a good method.
            //
            // 3. Copy the first element into a temporary variable. Iterate until the right place
            //    for it is found. As we go along, copy every traversed element into the slot
            //    preceding it. Finally, copy data from the temporary variable into the remaining
            //    hole. This method is very good. Benchmarks demonstrated slightly better
            //    performance than with the 2nd method.
            //
            // All methods were benchmarked, and the 3rd showed best results. So we chose that one.
            let mut tmp = NoDrop { value: Some(ptr::read(&v[0])) };

            // Intermediate state of the insertion process is always tracked by `hole`, which
            // serves two purposes:
            // 1. Protects integrity of `v` from panics in `is_less`.
            // 2. Fills the remaining hole in `v` in the end.
            //
            // Panic safety:
            //
            // If `is_less` panics at any point during the process, `hole` will get dropped and
            // fill the hole in `v` with `tmp`, thus ensuring that `v` still holds every object it
            // initially held exactly once.
            let mut hole = InsertionHole {
                src: tmp.value.as_mut().unwrap(),
                dest: &mut v[1],
            };
            ptr::copy_nonoverlapping(&v[1], &mut v[0], 1);

            for i in 2..v.len() {
                if !is_less(&v[i], &tmp.value.as_ref().unwrap()) {
                    break;
                }
                ptr::copy_nonoverlapping(&v[i], &mut v[i - 1], 1);
                hole.dest = &mut v[i];
            }
            // `hole` gets dropped and thus copies `tmp` into the remaining hole in `v`.
        }
    }

    // Holds a value, but never drops it.
    struct NoDrop<T> {
        value: Option<T>,
    }

    impl<T> Drop for NoDrop<T> {
        fn drop(&mut self) {
            mem::forget(self.value.take());
        }
    }

    // When dropped, copies from `src` into `dest`.
    struct InsertionHole<T> {
        src: *mut T,
        dest: *mut T,
    }

    impl<T> Drop for InsertionHole<T> {
        fn drop(&mut self) {
            unsafe { ptr::copy_nonoverlapping(self.src, self.dest, 1); }
        }
    }
}

/// Sorts a slice using insertion sort, which is `O(n^2)` worst-case.
fn insertion_sort<T, F>(v: &mut [T], is_less: &mut F)
    where F: FnMut(&T, &T) -> bool
{
    let len = v.len();
    if len >= 2 {
        for i in (0..len-1).rev() {
            insert_head(&mut v[i..], is_less);
        }
    }
}

/// Sorts `v` using heapsort, which guarantees `O(n log n)` worst-case.
#[cold]
fn heapsort<T, F>(v: &mut [T], is_less: &mut F)
    where F: FnMut(&T, &T) -> bool
{
    // This binary heap respects the invariant `parent >= child`.
    let mut sift_down = |v: &mut [T], mut node| {
        loop {
            // Children of `node`:
            let left = 2 * node + 1;
            let right = 2 * node + 2;

            // Choose the greater child.
            let greater = if right < v.len() && is_less(&v[left], &v[right]) {
                right
            } else {
                left
            };

            // Stop if the invariant holds at `node`.
            if greater >= v.len() || !is_less(&v[node], &v[greater]) {
                break;
            }

            // Swap `node` with the greater child, move one step down, and continue sifting.
            v.swap(node, greater);
            node = greater;
        }
    };

    // Build the heap in linear time.
    for i in (0 .. v.len() / 2).rev() {
        sift_down(v, i);
    }

    // Pop maximal elements from the heap.
    for i in (1 .. v.len()).rev() {
        v.swap(0, i);
        sift_down(&mut v[..i], 0);
    }
}

/// Partitions `v` into elements smaller than `pivot`, followed by elements greater than or equal
/// to `pivot`. Returns the number of elements smaller than `pivot`.
///
/// Partitioning is performed block-by-block in order to minimize the cost of branching operations.
/// This idea is presented in the [BlockQuicksort][pdf] paper.
///
/// [pdf]: http://drops.dagstuhl.de/opus/volltexte/2016/6389/pdf/LIPIcs-ESA-2016-38.pdf
fn partition_in_blocks<T, F>(v: &mut [T], pivot: &T, is_less: &mut F) -> usize
    where F: FnMut(&T, &T) -> bool
{
    const BLOCK: usize = 64;

    // State on the left side.
    let mut l = v.as_mut_ptr();
    let mut block_l = BLOCK;
    let mut start_l = ptr::null_mut();
    let mut end_l = ptr::null_mut();
    let mut offsets_l: [u8; BLOCK] = unsafe { mem::uninitialized() };

    // State on the right side.
    let mut r = unsafe { l.offset(v.len() as isize) };
    let mut block_r = BLOCK;
    let mut start_r = ptr::null_mut();
    let mut end_r = ptr::null_mut();
    let mut offsets_r: [u8; BLOCK] = unsafe { mem::uninitialized() };

    // Returns the number of elements between pointers `l` (inclusive) and `r` (exclusive).
    fn width<T>(l: *mut T, r: *mut T) -> usize {
        (r as usize - l as usize) / mem::size_of::<T>()
    }

    // Roughly speaking, the idea is to repeat the following steps until completion:
    //
    // 1. Trace a block from the left side to identify elements greater than or equal to the pivot.
    // 2. Trace a block from the right side to identify elements less than the pivot.
    // 3. Exchange the identified elements between the left and right side.
    loop {
        // We are done with partitioning block-by-block when `l` and `r` get very close. Then we do
        // some patch-up work in order to partition the remaining elements in between.
        let is_done = width(l, r) <= 2 * BLOCK;

        if is_done {
            // Number of remaining elements (still not compared to the pivot).
            let rem = width(l, r) - (start_l < end_l || start_r < end_r) as usize * BLOCK;

            // Adjust block sizes so that the left and right block don't overlap, but get perfectly
            // aligned to cover the whole remaining gap.
            if start_l < end_l {
                block_r = rem;
            } else if start_r < end_r {
                block_l = rem;
            } else {
                block_l = rem / 2;
                block_r = rem - block_l;
            }
            debug_assert!(width(l, r) == block_l + block_r);
        }

        if start_l == end_l {
            // Trace `block_l` elements from the left side.
            start_l = offsets_l.as_mut_ptr();
            end_l = offsets_l.as_mut_ptr();
            let mut elem = l;

            for i in 0..block_l {
                unsafe {
                    // Branchless comparison.
                    *end_l = i as u8;
                    end_l = end_l.offset(!is_less(&*elem, pivot) as isize);
                    elem = elem.offset(1);
                }
            }
        }

        if start_r == end_r {
            // Trace `block_r` elements from the right side.
            start_r = offsets_r.as_mut_ptr();
            end_r = offsets_r.as_mut_ptr();
            let mut elem = r;

            for i in 0..block_r {
                unsafe {
                    // Branchless comparison.
                    elem = elem.offset(-1);
                    *end_r = i as u8;
                    end_r = end_r.offset(is_less(&*elem, pivot) as isize);
                }
            }
        }

        // Number of displaced elements to swap between the left and right side.
        let count = cmp::min(width(start_l, end_l), width(start_r, end_r));

        if count > 0 {
            macro_rules! left { () => { l.offset(*start_l as isize) } }
            macro_rules! right { () => { r.offset(-(*start_r as isize) - 1) } }

            // Instead of swapping one pair at the time, it is more efficient to perform a cyclic
            // permutation. This is not strictly equivalent to swapping, but produces a similar
            // result using fewer memory operations.
            unsafe {
                let tmp = ptr::read(left!());
                ptr::copy_nonoverlapping(right!(), left!(), 1);

                for _ in 1..count {
                    start_l = start_l.offset(1);
                    ptr::copy_nonoverlapping(left!(), right!(), 1);
                    start_r = start_r.offset(1);
                    ptr::copy_nonoverlapping(right!(), left!(), 1);
                }

                ptr::copy_nonoverlapping(&tmp, right!(), 1);
                mem::forget(tmp);
                start_l = start_l.offset(1);
                start_r = start_r.offset(1);
            }
        }

        if start_l == end_l {
            // The left block is fully exhausted. Shift the left bound forward.
            l = unsafe { l.offset(block_l as isize) };
        }

        if start_r == end_r {
            // The right block is fully exhausted. Shift the right bound backward.
            r = unsafe { r.offset(-(block_r as isize)) };
        }

        if is_done {
            break;
        }
    }

    if start_l < end_l {
        // Move the remaining to-be-swapped elements to the far right.
        while start_l < end_l {
            unsafe {
                end_l = end_l.offset(-1);
                ptr::swap(l.offset(*end_l as isize), r.offset(-1));
                r = r.offset(-1);
            }
        }
        width(v.as_mut_ptr(), r)
    } else {
        // Move the remaining to-be-swapped elements to the far left.
        while start_r < end_r {
            unsafe {
                end_r = end_r.offset(-1);
                ptr::swap(l, r.offset(-(*end_r as isize) - 1));
                l = l.offset(1);
            }
        }
        width(v.as_mut_ptr(), l)
    }
}

/// Partitions `v` into elements smaller than `v[pivot]`, followed by elements greater than or
/// equal to `v[pivot]`.
///
/// Returns two things:
///
/// 1. Number of elements smaller than `v[pivot]`.
/// 2. `true` if `v` was already partitioned.
fn partition<T, F>(v: &mut [T], pivot: usize, is_less: &mut F) -> (usize, bool)
    where F: FnMut(&T, &T) -> bool
{
    v.swap(0, pivot);

    let (mid, was_partitioned) = {
        let (pivot, v) = v.split_at_mut(1);

        // Read the pivot into a stack-allocated variable for efficiency.
        let write_on_drop = WriteOnDrop {
            value: unsafe { Some(ptr::read(&pivot[0])) },
            dest: &mut pivot[0],
        };
        let pivot = write_on_drop.value.as_ref().unwrap();

        // Find the first pair of displaced elements.
        let mut l = 0;
        let mut r = v.len();
        unsafe {
            while l < r && is_less(v.get_unchecked(l), pivot) {
                l += 1;
            }
            while l < r && !is_less(v.get_unchecked(r - 1), pivot) {
                r -= 1;
            }
        }

        (l + partition_in_blocks(&mut v[l..r], pivot, is_less), l >= r)
    };

    v.swap(0, mid);
    (mid, was_partitioned)
}

/// Partitions `v` into elements equal to `v[pivot]` followed by elements greater than `v[pivot]`.
/// It is assumed that `v` does not contain elements smaller than `v[pivot]`.
fn partition_equal<T, F>(v: &mut [T], pivot: usize, is_less: &mut F) -> usize
    where F: FnMut(&T, &T) -> bool
{
    v.swap(0, pivot);
    let (pivot, v) = v.split_at_mut(1);

    // Read the pivot into a stack-allocated variable for efficiency.
    let write_on_drop = WriteOnDrop {
        value: unsafe { Some(ptr::read(&pivot[0])) },
        dest: &mut pivot[0],
    };
    let pivot = write_on_drop.value.as_ref().unwrap();

    let mut l = 0;
    let mut r = v.len();
    loop {
        unsafe {
            while l < r && !is_less(pivot, v.get_unchecked(l)) {
                l += 1;
            }
            while l < r && is_less(pivot, v.get_unchecked(r - 1)) {
                r -= 1;
            }
            if l >= r {
                break;
            }
            r -= 1;
            ptr::swap(v.get_unchecked_mut(l), v.get_unchecked_mut(r));
            l += 1;
        }
    }

    // Add 1 to also account for the pivot at index 0.
    l + 1
}

/// Scatters some elements around in an attempt to break patterns that might cause imbalanced
/// partitions in quicksort.
#[cold]
fn break_patterns<T>(v: &mut [T]) {
    let len = v.len();

    if len >= 8 {
        let mut rnd = len as u32;
        rnd ^= rnd << 13;
        rnd ^= rnd >> 17;
        rnd ^= rnd << 5;

        let mask = (len / 4).next_power_of_two() - 1;
        let rnd = rnd as usize & mask;
        debug_assert!(rnd < len / 2);

        let a = len / 4 * 2;
        let b = len / 4 + rnd;

        // Swap neighbourhoods of `a` and `b`.
        for i in 0..3 {
            v.swap(a - 1 + i, b - 1 + i);
        }
    }
}

/// Chooses a pivot in `v` and returns it's index.
///
/// Elements of `v` might be shuffled in the process.
fn choose_pivot<T, F>(v: &mut [T], is_less: &mut F) -> usize
    where F: FnMut(&T, &T) -> bool
{
    const SHORTEST_MEDIAN_OF_MEDIANS: usize = 90;
    const MAX_SWAPS: usize = 4 * 3;

    let len = v.len();
    let mut a = len / 4 * 1;
    let mut b = len / 4 * 2;
    let mut c = len / 4 * 3;
    let mut swaps = 0;

    if len >= 4 {
        let mut sort2 = |a: &mut usize, b: &mut usize| unsafe {
            if is_less(v.get_unchecked(*b), v.get_unchecked(*a)) {
                ptr::swap(a, b);
                swaps += 1;
            }
        };

        let mut sort3 = |a: &mut usize, b: &mut usize, c: &mut usize| {
            sort2(a, b);
            sort2(b, c);
            sort2(a, b);
        };

        if len >= SHORTEST_MEDIAN_OF_MEDIANS {
            let mut sort_adjacent = |a: &mut usize| {
                let tmp = *a;
                sort3(&mut (tmp - 1), a, &mut (tmp + 1));
            };

            sort_adjacent(&mut a);
            sort_adjacent(&mut b);
            sort_adjacent(&mut c);
        }
        sort3(&mut a, &mut b, &mut c);
    }

    if swaps < MAX_SWAPS {
        b
    } else {
        v.reverse();
        len - 1 - b
    }
}

/// Sorts `v` recursively.
///
/// If the slice had a predecessor in the original array, it is specified as `pred`.
///
/// `limit` is the number of allowed imbalanced partitions before switching to `heapsort`. If zero,
/// this function will immediately switch to heapsort.
fn recurse<T, F>(v: &mut [T], is_less: &mut F, pred: Option<&T>, mut limit: usize)
    where F: FnMut(&T, &T) -> bool
{
    // If `v` has length up to `insertion_len`, simply switch to insertion sort because it is going
    // to perform better than quicksort. For bigger types `T`, the threshold is smaller.
    let max_insertion = if mem::size_of::<T>() <= 2 * mem::size_of::<usize>() {
        32
    } else {
        16
    };

    let len = v.len();

    if len <= max_insertion {
        insertion_sort(v, is_less);
        return;
    }

    if limit == 0 {
        heapsort(v, is_less);
        return;
    }

    let pivot = choose_pivot(v, is_less);

    // If the chosen pivot is equal to the predecessor, then it's the smallest element in the
    // slice. Partition the slice into elements equal to and elements greater than the pivot.
    // This case is often hit when the slice contains many duplicate elements.
    if let Some(p) = pred {
        if !is_less(p, &v[pivot]) {
            let mid = partition_equal(v, pivot, is_less);
            recurse(&mut v[mid..], is_less, pred, limit);
            return;
        }
    }

    let (mid, was_partitioned) = partition(v, pivot, is_less);
    let is_balanced = cmp::min(mid, len - mid) >= len / 8;

    // If the partitioning is decently balanced and the slice was already partitioned, there are
    // good chances it is also completely sorted. If so, we're done.
    if is_balanced && was_partitioned && v.windows(2).all(|w| !is_less(&w[1], &w[0])) {
        return;
    }

    let (left, right) = v.split_at_mut(mid);
    let (pivot, right) = right.split_at_mut(1);

    // If the partitioning is imbalanced, try breaking patterns in the slice by shuffling
    // potential future pivots around.
    if !is_balanced {
        break_patterns(left);
        break_patterns(right);
        limit -= 1;
    }

    recurse(left, is_less, pred, limit);
    recurse(right, is_less, Some(&pivot[0]), limit);
}

/// Sorts `v` using quicksort.
fn quicksort<T, F>(v: &mut [T], mut is_less: F)
    where F: FnMut(&T, &T) -> bool
{
    // Sorting has no meaningful behavior on zero-sized types.
    if mem::size_of::<T>() == 0 {
        return;
    }

    let limit = 64 - (v.len() as u64).leading_zeros() as usize + 1;
    recurse(v, &mut is_less, None, limit);
}

/// Sorts a slice.
///
/// This sort is in-place, unstable, and `O(n log n)` worst-case.
///
/// The implementation is based on Orson Peters' pattern-defeating quicksort.
///
/// # Examples
///
/// ```
/// extern crate pdqsort;
///
/// let mut v = [-5, 4, 1, -3, 2];
/// pdqsort::sort(&mut v);
/// assert!(v == [-5, -3, 1, 2, 4]);
/// ```
#[inline]
pub fn sort<T>(v: &mut [T])
    where T: Ord
{
    quicksort(v, |a, b| a.lt(b));
}

/// Sorts a slice using `f` to extract a key to compare elements by.
///
/// This sort is in-place, unstable, and `O(n log n)` worst-case.
///
/// The implementation is based on Orson Peters' pattern-defeating quicksort.
///
/// # Examples
///
/// ```
/// extern crate pdqsort;
///
/// let mut v = [-5i32, 4, 1, -3, 2];
/// pdqsort::sort_by_key(&mut v, |k| k.abs());
/// assert!(v == [1, 2, -3, 4, -5]);
/// ```
#[inline]
pub fn sort_by_key<T, B, F>(v: &mut [T], mut f: F)
    where F: FnMut(&T) -> B,
          B: Ord
{
    quicksort(v, |a, b| f(a).lt(&f(b)))
}

/// Sorts a slice using `compare` to compare elements.
///
/// This sort is in-place, unstable, and `O(n log n)` worst-case.
///
/// The implementation is based on Orson Peters' pattern-defeating quicksort.
///
/// # Examples
///
/// ```
/// extern crate pdqsort;
///
/// let mut v = [5, 4, 1, 3, 2];
/// pdqsort::sort_by(&mut v, |a, b| a.cmp(b));
/// assert!(v == [1, 2, 3, 4, 5]);
///
/// // reverse sorting
/// pdqsort::sort_by(&mut v, |a, b| b.cmp(a));
/// assert!(v == [5, 4, 3, 2, 1]);
/// ```
#[inline]
pub fn sort_by<T, F>(v: &mut [T], mut compare: F)
    where F: FnMut(&T, &T) -> Ordering
{
    quicksort(v, |a, b| compare(a, b) == Ordering::Less);
}

#[cfg(test)]
mod tests {
    extern crate std;
    extern crate rand;

    use self::rand::{Rng, thread_rng};
    use self::std::cmp::Ordering::{Greater, Less};
    use self::std::prelude::v1::*;

    #[test]
    fn test_sort_zero_sized_type() {
        // Should not panic.
        [(); 10].sort();
        [(); 100].sort();
    }

    #[test]
    fn test_pdqsort() {
        let mut rng = thread_rng();
        for n in 0..16 {
            for l in 0..16 {
                let mut v = rng.gen_iter::<u64>()
                    .map(|x| x % (1 << l))
                    .take((1 << n))
                    .collect::<Vec<_>>();
                let mut v1 = v.clone();

                super::sort(&mut v);
                assert!(v.windows(2).all(|w| w[0] <= w[1]));

                v1.sort_by(|a, b| a.cmp(b));
                assert!(v1.windows(2).all(|w| w[0] <= w[1]));

                v1.sort_by(|a, b| b.cmp(a));
                assert!(v1.windows(2).all(|w| w[0] >= w[1]));
            }
        }

        let mut v = [0xDEADBEEFu64];
        super::sort(&mut v);
        assert!(v == [0xDEADBEEF]);
    }

    #[test]
    fn test_heapsort() {
        let mut rng = thread_rng();
        for n in 0..16 {
            for l in 0..16 {
                let mut v = rng.gen_iter::<u64>()
                    .map(|x| x % (1 << l))
                    .take((1 << n))
                    .collect::<Vec<_>>();
                let mut v1 = v.clone();

                super::heapsort(&mut v, &mut |a, b| a.lt(b));
                assert!(v.windows(2).all(|w| w[0] <= w[1]));

                v1.sort_by(|a, b| a.cmp(b));
                assert!(v1.windows(2).all(|w| w[0] <= w[1]));

                v1.sort_by(|a, b| b.cmp(a));
                assert!(v1.windows(2).all(|w| w[0] >= w[1]));
            }
        }

        let mut v = [0xDEADBEEFu64];
        super::heapsort(&mut v, &mut |a, b| a.lt(b));
        assert!(v == [0xDEADBEEF]);
    }

    #[test]
    fn test_crazy_compare() {
        let mut rng = thread_rng();

        let mut v = rng.gen_iter::<u64>()
            .map(|x| x % 1000)
            .take(100_000)
            .collect::<Vec<_>>();

        // Even though comparison is non-sensical, sorting must not panic.
        super::sort_by(&mut v, |_, _| *rng.choose(&[Less, Greater]).unwrap());
    }
}
