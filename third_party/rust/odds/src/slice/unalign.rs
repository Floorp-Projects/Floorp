

use std::mem::size_of;
use std::mem::uninitialized;
use std::marker::PhantomData;
use std::ptr;

use slice::Pod;
use slice::iter::SliceCopyIter;

/// An iterator of `T` (by value) where each value read using an
/// unaligned load.
///
/// See also the method `.tail()`.
#[derive(Debug)]
pub struct UnalignedIter<'a, T: 'a> {
    ptr: *const u8,
    end: *const u8,
    tail_end: *const u8,
    ty: PhantomData<&'a T>,
}

impl<'a, T> Copy for UnalignedIter<'a, T> { }
impl<'a, T> Clone for UnalignedIter<'a, T> {
    fn clone(&self) -> Self { *self }
}

impl<'a, T> UnalignedIter<'a, T> {
    /// Create an `UnalignedIter` from `ptr` and `end`, which must be spaced
    /// an whole number of `T` offsets apart.
    pub unsafe fn from_raw_parts(ptr: *const u8, end: *const u8) -> Self {
        let len = end as usize - ptr as usize;
        debug_assert_eq!(len % size_of::<T>(), 0);
        UnalignedIter {
            ptr: ptr,
            end: end,
            tail_end: end,
            ty: PhantomData,
        }
    }

    /// Create an `UnalignedIter` out of the slice of data, which
    /// iterates first in blocks of `T` (unaligned loads), and
    /// then leaves a tail of the remaining bytes.
    pub fn from_slice(data: &'a [u8]) -> Self where T: Pod {
        unsafe {
            let ptr = data.as_ptr();
            let len = data.len();
            let sz = size_of::<T>() as isize;
            let end_block = ptr.offset(len as isize / sz * sz);
            let end = ptr.offset(len as isize);
            UnalignedIter {
                ptr: ptr,
                end: end_block,
                tail_end: end,
                ty: PhantomData,
            }
        }
    }

    /// Return a byte iterator of the remaining tail of the iterator;
    /// this can be called at any time, but in particular when the iterator
    /// has returned None.
    pub fn tail(&self) -> SliceCopyIter<'a, u8> {
        unsafe {
            SliceCopyIter::new(self.ptr, self.tail_end)
        }
    }

    /// Return `true` if the tail is not empty.
    pub fn has_tail(&self) -> bool {
        self.ptr != self.tail_end
    }

    /// Return the next iterator element, without stepping the iterator.
    pub fn peek_next(&self) -> Option<T> where T: Copy {
        if self.ptr != self.end {
            unsafe {
                Some(load_unaligned(self.ptr))
            }
        } else {
            None
        }
    }
}

unsafe fn load_unaligned<T>(p: *const u8) -> T where T: Copy {
    let mut x = uninitialized();
    ptr::copy_nonoverlapping(p, &mut x as *mut _ as *mut u8, size_of::<T>());
    x
}

impl<'a, T> Iterator for UnalignedIter<'a, T>
    where T: Copy,
{
    type Item = T;
    fn next(&mut self) -> Option<Self::Item> {
        if self.ptr != self.end {
            unsafe {
                let elt = Some(load_unaligned::<T>(self.ptr));
                self.ptr = self.ptr.offset(size_of::<T>() as isize);
                elt
            }
        } else {
            None
        }
    }
}


#[test]
fn test_unalign() {
    let data = [0u8, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    let mut iter = UnalignedIter::<u32>::from_slice(&data);
    assert_eq!(iter.next(), Some(u32::from_be(0x00010203)));
    assert_eq!(iter.next(), Some(u32::from_be(0x04050607)));
    let mut tail = iter.tail();
    assert_eq!(tail.next(), Some(8));
    assert_eq!(tail.next(), Some(9));
    assert_eq!(tail.next(), None);
}
