//! Slice iterator with custom step size
//!
//! Performance note: Using stable Rust features, these iterators
//! don't quite live up to the efficiency that they should have,
//! unfortunately.
//!
//! Licensed under the Apache License, Version 2.0
//! http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
//! http://opensource.org/licenses/MIT, at your
//! option. This file may not be copied, modified, or distributed
//! except according to those terms.

use std::fmt;
use std::marker;
use std::mem;
use std::ops::{Index, IndexMut};


/// (the stride) skipped per iteration.
///
/// `Stride` does not support zero-sized types for `A`.
///
/// Iterator element type is `&'a A`.
pub struct Stride<'a, A: 'a> {
    /// base pointer -- does not change during iteration
    begin: *const A,
    /// current offset from begin
    offset: isize,
    /// offset where we end (exclusive end).
    end: isize,
    stride: isize,
    life: marker::PhantomData<&'a A>,
}

impl<'a, A> Copy for Stride<'a, A> {}
unsafe impl<'a, A> Send for Stride<'a, A> where A: Sync {}
unsafe impl<'a, A> Sync for Stride<'a, A> where A: Sync {}

/// The mutable equivalent of Stride.
///
/// `StrideMut` does not support zero-sized types for `A`.
///
/// Iterator element type is `&'a mut A`.
pub struct StrideMut<'a, A: 'a> {
    begin: *mut A,
    offset: isize,
    end: isize,
    stride: isize,
    life: marker::PhantomData<&'a mut A>,
}

unsafe impl<'a, A> Send for StrideMut<'a, A> where A: Send {}
unsafe impl<'a, A> Sync for StrideMut<'a, A> where A: Sync {}

impl<'a, A> Stride<'a, A> {
    /// Create a Stride iterator from a raw pointer.
    pub unsafe fn from_ptr_len(begin: *const A, nelem: usize, stride: isize) -> Stride<'a, A>
    {
        Stride {
            begin: begin,
            offset: 0,
            end: stride * nelem as isize,
            stride: stride,
            life: marker::PhantomData,
        }
    }
}

impl<'a, A> StrideMut<'a, A>
{
    /// Create a StrideMut iterator from a raw pointer.
    pub unsafe fn from_ptr_len(begin: *mut A, nelem: usize, stride: isize) -> StrideMut<'a, A>
    {
        StrideMut {
            begin: begin,
            offset: 0,
            end: stride * nelem as isize,
            stride: stride,
            life: marker::PhantomData,
        }
    }
}

fn div_rem(x: usize, d: usize) -> (usize, usize) {
    (x / d, x % d)
}

macro_rules! stride_impl {
    (struct $name:ident -> $slice:ty, $getptr:ident, $ptr:ty, $elem:ty) => {
        impl<'a, A> $name<'a, A>
        {
            /// Create Stride iterator from a slice and the element step count.
            ///
            /// If `step` is negative, start from the back.
            ///
            /// ```
            /// use odds::stride::Stride;
            ///
            /// let xs = [0, 1, 2, 3, 4, 5];
            ///
            /// let front = Stride::from_slice(&xs, 2);
            /// assert_eq!(front[0], 0);
            /// assert_eq!(front[1], 2);
            ///
            /// let back = Stride::from_slice(&xs, -2);
            /// assert_eq!(back[0], 5);
            /// assert_eq!(back[1], 3);
            /// ```
            ///
            /// **Panics** if values of type `A` are zero-sized. <br>
            /// **Panics** if `step` is 0.
            #[inline]
            pub fn from_slice(xs: $slice, step: isize) -> $name<'a, A>
            {
                assert!(mem::size_of::<A>() != 0);
                let ustep = if step < 0 { -step } else { step } as usize;
                let nelem = if ustep <= 1 {
                    xs.len()
                } else {
                    let (d, r) = div_rem(xs.len(), ustep);
                    d + if r > 0 { 1 } else { 0 }
                };
                let mut begin = xs. $getptr ();
                unsafe {
                    if step > 0 {
                        $name::from_ptr_len(begin, nelem, step)
                    } else {
                        if nelem != 0 {
                            begin = begin.offset(xs.len() as isize - 1)
                        }
                        $name::from_ptr_len(begin, nelem, step)
                    }
                }
            }

            /// Create Stride iterator from an existing Stride iterator
            ///
            /// **Panics** if `step` is 0.
            #[inline]
            pub fn from_stride(mut it: $name<'a, A>, mut step: isize) -> $name<'a, A>
            {
                assert!(step != 0);
                if step < 0 {
                    it.swap_ends();
                    step = -step;
                }
                let len = (it.end - it.offset) / it.stride;
                let newstride = it.stride * step;
                let (d, r) = div_rem(len as usize, step as usize);
                let len = d + if r > 0 { 1 } else { 0 };
                unsafe {
                    $name::from_ptr_len(it.begin, len, newstride)
                }
            }

            /// Swap the begin and end and reverse the stride,
            /// in effect reversing the iterator.
            #[inline]
            pub fn swap_ends(&mut self) {
                let len = (self.end - self.offset) / self.stride;
                if len > 0 {
                    unsafe {
                        let endptr = self.begin.offset((len - 1) * self.stride);
                        *self = $name::from_ptr_len(endptr, len as usize, -self.stride);
                    }
                }
            }

            /// Return the number of elements in the iterator.
            #[inline]
            pub fn len(&self) -> usize {
                ((self.end - self.offset) / self.stride) as usize
            }

            /// Return a reference to the element of a stride at the
            /// given index, or None if the index is out of bounds.
            #[inline]
            pub fn get<'b>(&'b self, i: usize) -> Option<&'b A> {
                if i >= self.len() {
                    None
                } else {
                    unsafe {
                        let ptr = self.begin.offset(self.offset + self.stride * (i as isize));
                        Some(mem::transmute(ptr))
                    }
                }
            }
        }

        impl<'a, A> Iterator for $name<'a, A>
        {
            type Item = $elem;
            #[inline]
            fn next(&mut self) -> Option<$elem>
            {
                if self.offset == self.end {
                    None
                } else {
                    unsafe {
                        let elt: $elem =
                            mem::transmute(self.begin.offset(self.offset));
                        self.offset += self.stride;
                        Some(elt)
                    }
                }
            }

            #[inline]
            fn size_hint(&self) -> (usize, Option<usize>) {
                let len = self.len();
                (len, Some(len))
            }
        }

        impl<'a, A> DoubleEndedIterator for $name<'a, A>
        {
            #[inline]
            fn next_back(&mut self) -> Option<$elem>
            {
                if self.offset == self.end {
                    None
                } else {
                    unsafe {
                        self.end -= self.stride;
                        let elt = mem::transmute(self.begin.offset(self.end));
                        Some(elt)
                    }
                }
            }
        }

        impl<'a, A> ExactSizeIterator for $name<'a, A> { }

        impl<'a, A> Index<usize> for $name<'a, A>
        {
            type Output = A;
            /// Return a reference to the element at a given index.
            ///
            /// **Panics** if the index is out of bounds.
            fn index<'b>(&'b self, i: usize) -> &'b A
            {
                assert!(i < self.len());
                unsafe {
                    let ptr = self.begin.offset(self.offset + self.stride * (i as isize));
                    mem::transmute(ptr)
                }
            }
        }

        impl<'a, A> fmt::Debug for $name<'a, A>
            where A: fmt::Debug
        {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result
            {
                try!(write!(f, "["));
                for i in 0..self.len() {
                    if i != 0 {
                        try!(write!(f, ", "));
                    }
                    try!(write!(f, "{:?}", (*self)[i]));
                }
                write!(f, "]")
            }
        }
    }
}

stride_impl!{struct Stride -> &'a [A], as_ptr, *const A, &'a A}
stride_impl!{struct StrideMut -> &'a mut [A], as_mut_ptr, *mut A, &'a mut A}

impl<'a, A> Clone for Stride<'a, A> {
    fn clone(&self) -> Stride<'a, A> {
        *self
    }
}

impl<'a, A> StrideMut<'a, A> {
    /// Return a mutable reference to the element of a stride at the
    /// given index, or None if the index is out of bounds.
    pub fn get_mut<'b>(&'b mut self, i: usize) -> Option<&'b mut A> {
        if i >= self.len() {
            None
        } else {
            unsafe {
                let ptr = self.begin.offset(self.offset + self.stride * (i as isize));
                Some(&mut *ptr)
            }
        }
    }
}

impl<'a, A> IndexMut<usize> for StrideMut<'a, A> {
    /// Return a mutable reference to the element at a given index.
    ///
    /// **Panics** if the index is out of bounds.
    fn index_mut<'b>(&'b mut self, i: usize) -> &'b mut A {
        assert!(i < self.len());
        unsafe {
            let ptr = self.begin.offset(self.offset + self.stride * (i as isize));
            &mut *ptr
        }
    }
}
