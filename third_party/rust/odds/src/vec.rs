//! Extensions to `Vec`
//!
//! Requires `feature="std"`
#![cfg(feature="std")]

use range::IndexRange;
use std::ptr;
use std::slice;

use slice::SliceFind;


/// Create a new vec from the iterable
pub fn vec<I>(iterable: I) -> Vec<I::Item>
    where I: IntoIterator
{
    iterable.into_iter().collect()
}


/// Extra methods for `Vec<T>`
///
/// Requires `feature="std"`
pub trait VecExt<T> {
    /// Remove elements in a range, and insert from an iterator in their place.
    ///
    /// The removed and inserted ranges don't have to match in length.
    ///
    /// **Panics** if range `r` is out of bounds.
    ///
    /// **Panics** if iterator `iter` is not of exact length.
    fn splice<R, I>(&mut self, r: R, iter: I)
        where I: IntoIterator<Item=T>,
              I::IntoIter: ExactSizeIterator,
              R: IndexRange;

    /// Retains only the elements specified by the predicate.
    ///
    /// In other words, remove all elements `e` such that `f(&mut e)` returns false.
    /// This method operates in place and preserves the order of the retained
    /// elements.
    ///
    /// # Examples
    ///
    /// ```
    /// use odds::vec::VecExt;
    /// let mut vec = vec![1, 2, 3, 4];
    /// vec.retain_mut(|x| {
    ///     let keep = *x % 2 == 0;
    ///     *x *= 10;
    ///     keep
    /// });
    /// assert_eq!(vec, [20, 40]);
    /// ```
    fn retain_mut<F>(&mut self, f: F)
        where F: FnMut(&mut T) -> bool;
}

/// `Vec::splice`: Remove elements in a range, and insert from an iterator
/// in their place.
///
/// The removed and inserted ranges don't have to match in length.
///
/// **Panics** if range `r` is out of bounds.
///
/// **Panics** if iterator `iter` is not of exact length.
impl<T> VecExt<T> for Vec<T> {
    fn splice<R, I>(&mut self, r: R, iter: I)
        where I: IntoIterator<Item=T>,
              I::IntoIter: ExactSizeIterator,
              R: IndexRange,
    {
        let v = self;
        let mut iter = iter.into_iter();
        let (input_len, _) = iter.size_hint();
        let old_len = v.len();
        let r = r.start().unwrap_or(0)..r.end().unwrap_or(old_len);
        assert!(r.start <= r.end);
        assert!(r.end <= v.len());
        let rm_len = r.end - r.start;
        v.reserve(input_len.saturating_sub(rm_len));

        unsafe {
            let ptr = v.as_mut_ptr();
            v.set_len(r.start);

            // drop all elements in `r`
            {
                let mslc = slice::from_raw_parts_mut(ptr.offset(r.start as isize), rm_len);
                for elt_ptr in mslc {
                    ptr::read(elt_ptr); // Possible panic
                }
            }

            if rm_len != input_len {
                // move tail elements
                ptr::copy(ptr.offset(r.end as isize),
                          ptr.offset((r.start + input_len) as isize),
                          old_len - r.end);
            }

            // fill in elements from the iterator
            // FIXME: On panic, drop tail properly too (using panic guard)
            {
                let grow_slc = slice::from_raw_parts_mut(ptr.offset(r.start as isize), input_len);
                let mut len = r.start;
                for slot_ptr in grow_slc {
                    if let Some(input_elt) = iter.next() { // Possible Panic
                        ptr::write(slot_ptr, input_elt);
                    } else {
                        // FIXME: Skip check with trusted iterators
                        panic!("splice: iterator too short");
                    }
                    // update length to drop as much as possible on panic
                    len += 1;
                    v.set_len(len);
                }
                v.set_len(old_len - rm_len + input_len);
            }
        }
        //assert!(iter.next().is_none(), "splice: iterator not exact size");
    }

    // Adapted from libcollections/vec.rs in Rust
    // Primary author in Rust: Michael Darakananda
    fn retain_mut<F>(&mut self, mut f: F)
        where F: FnMut(&mut T) -> bool
    {
        let len = self.len();
        let mut del = 0;
        {
            let v = &mut **self;

            for i in 0..len {
                if !f(&mut v[i]) {
                    del += 1;
                } else if del > 0 {
                    v.swap(i - del, i);
                }
            }
        }
        if del > 0 {
            self.truncate(len - del);
        }
    }
}

pub trait VecFindRemove {
    type Item;
    /// Linear search for the first element equal to `elt` and remove
    /// it if found.
    ///
    /// Return its index and the value itself.
    fn find_remove<U>(&mut self, elt: &U) -> Option<(usize, Self::Item)>
        where Self::Item: PartialEq<U>;

    /// Linear search for the last element equal to `elt` and remove
    /// it if found.
    ///
    /// Return its index and the value itself.
    fn rfind_remove<U>(&mut self, elt: &U) -> Option<(usize, Self::Item)>
        where Self::Item: PartialEq<U>;
}

impl<T> VecFindRemove for Vec<T> {
    type Item = T;
    fn find_remove<U>(&mut self, elt: &U) -> Option<(usize, Self::Item)>
        where Self::Item: PartialEq<U>
    {
        self.find(elt).map(|i| (i, self.remove(i)))
    }
    fn rfind_remove<U>(&mut self, elt: &U) -> Option<(usize, Self::Item)>
        where Self::Item: PartialEq<U>
    {
        self.rfind(elt).map(|i| (i, self.remove(i)))
    }
}

#[test]
fn test_splice() {
    use std::iter::once;

    let mut v = vec![1, 2, 3, 4];
    v.splice(1..1, vec![9, 9]);
    assert_eq!(v, &[1, 9, 9, 2, 3, 4]);

    let mut v = vec![1, 2, 3, 4];
    v.splice(1..2, vec![9, 9]);
    assert_eq!(v, &[1, 9, 9, 3, 4]);

    let mut v = vec![1, 2, 3, 4, 5];
    v.splice(1..4, vec![9, 9]);
    assert_eq!(v, &[1, 9, 9, 5]);

    let mut v = vec![1, 2, 3, 4];
    v.splice(0..4, once(9));
    assert_eq!(v, &[9]);

    let mut v = vec![1, 2, 3, 4];
    v.splice(0..4, None);
    assert_eq!(v, &[]);

    let mut v = vec![1, 2, 3, 4];
    v.splice(1.., Some(9));
    assert_eq!(v, &[1, 9]);
}

#[test]
fn test_find() {
    let mut v = vec![0, 1, 2, 3, 1, 2, 1];
    assert_eq!(v.rfind_remove(&1), Some((6, 1)));
    assert_eq!(v.find_remove(&2), Some((2, 2)));
    assert_eq!(v.find_remove(&7), None);
    assert_eq!(&v, &[0, 1, 3, 1, 2]);
}
