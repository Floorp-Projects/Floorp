
use std::mem::size_of;
use std::marker::PhantomData;

/// Slice (contiguous data) iterator.
///
/// Iterator element type is `T` (by value).
/// This iterator exists mainly to have the constructor from a pair
/// of raw pointers available, which the libcore slice iterator does not allow.
///
/// Implementation note: Aliasing/optimization issues disappear if we use
/// non-pointer iterator element type, so we use `T`. (The libcore slice
/// iterator has `assume` and other tools available to combat it).
///
/// `T` must not be a zero sized type.
#[derive(Debug)]
pub struct SliceCopyIter<'a, T: 'a> {
    ptr: *const T,
    end: *const T,
    ty: PhantomData<&'a T>,
}

impl<'a, T> Copy for SliceCopyIter<'a, T> { }
impl<'a, T> Clone for SliceCopyIter<'a, T> {
    fn clone(&self) -> Self { *self }
}

impl<'a, T> SliceCopyIter<'a, T>
    where T: Copy
{
    #[inline]
    pub unsafe fn new(ptr: *const T, end: *const T) -> Self {
        assert!(size_of::<T>() != 0);
        SliceCopyIter {
            ptr: ptr,
            end: end,
            ty: PhantomData,
        }
    }

    /// Return the start, end pointer of the iterator
    pub fn into_raw(self) -> (*const T, *const T) {
        (self.ptr, self.end)
    }
}

impl<'a, T> Iterator for SliceCopyIter<'a, T>
    where T: Copy,
{
    type Item = T;
    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        if self.ptr != self.end {
            unsafe {
                let elt = Some(*self.ptr);
                self.ptr = self.ptr.offset(1);
                elt
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = (self.end as usize - self.ptr as usize) / size_of::<T>();
        (len, Some(len))
    }

    fn count(self) -> usize {
        self.len()
    }

    fn last(mut self) -> Option<Self::Item> {
        self.next_back()
    }
}

impl<'a, T> DoubleEndedIterator for SliceCopyIter<'a, T>
    where T: Copy
{
    #[inline]
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.ptr != self.end {
            unsafe {
                self.end = self.end.offset(-1);
                let elt = Some(*self.end);
                elt
            }
        } else {
            None
        }
    }
}

impl<'a, T> ExactSizeIterator for SliceCopyIter<'a, T> where T: Copy { }

impl<'a, T> From<&'a [T]> for SliceCopyIter<'a, T>
    where T: Copy
{
    fn from(slice: &'a [T]) -> Self {
        assert!(size_of::<T>() != 0);
        unsafe {
            let ptr = slice.as_ptr();
            let end = ptr.offset(slice.len() as isize);
            SliceCopyIter::new(ptr, end)
        }
    }
}

impl<'a, T> Default for SliceCopyIter<'a, T>
    where T: Copy
{
    /// Create an empty `SliceCopyIter`.
    fn default() -> Self {
        unsafe {
            SliceCopyIter::new(0x1 as *const T, 0x1 as *const T)
        }
    }
}
