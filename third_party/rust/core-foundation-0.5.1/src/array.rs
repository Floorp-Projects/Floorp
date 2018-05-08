// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Heterogeneous immutable arrays.

pub use core_foundation_sys::array::*;
pub use core_foundation_sys::base::CFIndex;
use core_foundation_sys::base::{CFTypeRef, CFRelease, kCFAllocatorDefault};
use libc::c_void;
use std::mem;
use std::mem::ManuallyDrop;
use std::marker::PhantomData;
use std;
use std::ops::Deref;
use std::fmt::{Debug, Formatter};

use base::{CFIndexConvertible, TCFType, TCFTypeRef, CFRange};

/// A heterogeneous immutable array.
pub struct CFArray<T = *const c_void>(CFArrayRef, PhantomData<T>);

/// A reference to an element inside the array
pub struct ItemRef<'a, T: 'a>(ManuallyDrop<T>, PhantomData<&'a T>);

impl<'a, T> Deref for ItemRef<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<'a, T: Debug> Debug for ItemRef<'a, T> {
    fn fmt(&self, f: &mut Formatter) -> Result<(), std::fmt::Error> {
        self.0.fmt(f)
    }
}

/// A trait describing how to convert from the stored *const c_void to the desired T
pub unsafe trait FromVoid {
    unsafe fn from_void<'a>(x: *const c_void) -> ItemRef<'a, Self> where Self: std::marker::Sized;
}

unsafe impl FromVoid for u32 {
    unsafe fn from_void<'a>(x: *const c_void) -> ItemRef<'a, Self> {
        // Functions like CGFontCopyTableTags treat the void*'s as u32's
        // so we convert by casting directly
        ItemRef(ManuallyDrop::new(x as u32), PhantomData)
    }
}

unsafe impl<T: TCFType> FromVoid for T {
    unsafe fn from_void<'a>(x: *const c_void) -> ItemRef<'a, Self> {
        ItemRef(ManuallyDrop::new(TCFType::wrap_under_create_rule(T::Ref::from_void_ptr(x))), PhantomData)
    }
}

impl<T> Drop for CFArray<T> {
    fn drop(&mut self) {
        unsafe { CFRelease(self.as_CFTypeRef()) }
    }
}

pub struct CFArrayIterator<'a, T: 'a> {
    array: &'a CFArray<T>,
    index: CFIndex,
    len: CFIndex,
}

impl<'a, T: FromVoid> Iterator for CFArrayIterator<'a, T> {
    type Item = ItemRef<'a, T>;

    fn next(&mut self) -> Option<ItemRef<'a, T>> {
        if self.index >= self.len {
            None
        } else {
            let value = unsafe { self.array.get_unchecked(self.index) };
            self.index += 1;
            Some(value)
        }
    }
}

impl<'a, T: FromVoid> ExactSizeIterator for CFArrayIterator<'a, T> {
    fn len(&self) -> usize {
        (self.array.len() - self.index) as usize
    }
}

impl_TCFTypeGeneric!(CFArray, CFArrayRef, CFArrayGetTypeID);
impl_CFTypeDescriptionGeneric!(CFArray);

impl<T> CFArray<T> {
    /// Creates a new `CFArray` with the given elements, which must be `CFType` objects.
    pub fn from_CFTypes(elems: &[T]) -> CFArray<T> where T: TCFType {
        unsafe {
            let elems: Vec<CFTypeRef> = elems.iter().map(|elem| elem.as_CFTypeRef()).collect();
            let array_ref = CFArrayCreate(kCFAllocatorDefault,
                                          mem::transmute(elems.as_ptr()),
                                          elems.len().to_CFIndex(),
                                          &kCFTypeArrayCallBacks);
            TCFType::wrap_under_create_rule(array_ref)
        }
    }

    #[inline]
    pub fn to_untyped(&self) -> CFArray {
        unsafe { CFArray::wrap_under_get_rule(self.0) }
    }

    /// Returns the same array, but with the type reset to void pointers.
    /// Equal to `to_untyped`, but is faster since it does not increment the retain count.
    #[inline]
    pub fn into_untyped(self) -> CFArray {
        let reference = self.0;
        mem::forget(self);
        unsafe { CFArray::wrap_under_create_rule(reference) }
    }

    /// Iterates over the elements of this `CFArray`.
    ///
    /// Careful; the loop body must wrap the reference properly. Generally, when array elements are
    /// Core Foundation objects (not always true), they need to be wrapped with
    /// `TCFType::wrap_under_get_rule()`.
    #[inline]
    pub fn iter<'a>(&'a self) -> CFArrayIterator<'a, T> {
        CFArrayIterator {
            array: self,
            index: 0,
            len: self.len(),
        }
    }

    #[inline]
    pub fn len(&self) -> CFIndex {
        unsafe {
            CFArrayGetCount(self.0)
        }
    }

    #[inline]
    pub unsafe fn get_unchecked<'a>(&'a self, index: CFIndex) -> ItemRef<'a, T> where T: FromVoid {
        T::from_void(CFArrayGetValueAtIndex(self.0, index))
    }

    #[inline]
    pub fn get<'a>(&'a self, index: CFIndex) -> Option<ItemRef<'a, T>> where T: FromVoid {
        if index < self.len() {
            Some(unsafe { T::from_void(CFArrayGetValueAtIndex(self.0, index)) } )
        } else {
            None
        }
    }

    pub fn get_values(&self, range: CFRange) -> Vec<*const c_void> {
        let mut vec = Vec::with_capacity(range.length as usize);
        unsafe {
            CFArrayGetValues(self.0, range, vec.as_mut_ptr());
            vec.set_len(range.length as usize);
            vec
        }
    }

    pub fn get_all_values(&self) -> Vec<*const c_void> {
        self.get_values(CFRange {
            location: 0,
            length: self.len()
        })
    }
}

impl<'a, T: FromVoid> IntoIterator for &'a CFArray<T> {
    type Item = ItemRef<'a, T>;
    type IntoIter = CFArrayIterator<'a, T>;

    fn into_iter(self) -> CFArrayIterator<'a, T> {
        self.iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem;
    use base::CFType;

    #[test]
    fn to_untyped_correct_retain_count() {
        let array = CFArray::<CFType>::from_CFTypes(&[]);
        assert_eq!(array.retain_count(), 1);

        let untyped_array = array.to_untyped();
        assert_eq!(array.retain_count(), 2);
        assert_eq!(untyped_array.retain_count(), 2);

        mem::drop(array);
        assert_eq!(untyped_array.retain_count(), 1);
    }

    #[test]
    fn into_untyped() {
        let array = CFArray::<CFType>::from_CFTypes(&[]);
        let array2 = array.to_untyped();
        assert_eq!(array.retain_count(), 2);

        let untyped_array = array.into_untyped();
        assert_eq!(untyped_array.retain_count(), 2);

        mem::drop(array2);
        assert_eq!(untyped_array.retain_count(), 1);
    }

    #[test]
    fn borrow() {
        use string::CFString;

        let string = CFString::from_static_string("bar");
        assert_eq!(string.retain_count(), 1);
        let x;
        {
            let arr: CFArray<CFString> = CFArray::from_CFTypes(&[string]);
            {
                let p = arr.get(0).unwrap();
                assert_eq!(p.retain_count(), 1);
            }
            {
                x = arr.get(0).unwrap().clone();
                assert_eq!(x.retain_count(), 2);
                assert_eq!(x.to_string(), "bar");
            }
        }
        assert_eq!(x.retain_count(), 1);
    }

    #[test]
    fn should_box_and_unbox() {
        use number::CFNumber;

        let n0 = CFNumber::from(0);
        let n1 = CFNumber::from(1);
        let n2 = CFNumber::from(2);
        let n3 = CFNumber::from(3);
        let n4 = CFNumber::from(4);
        let n5 = CFNumber::from(5);

        let arr = CFArray::from_CFTypes(&[
            n0.as_CFType(),
            n1.as_CFType(),
            n2.as_CFType(),
            n3.as_CFType(),
            n4.as_CFType(),
            n5.as_CFType(),
        ]);

        assert!(arr.get_all_values() == &[n0.as_CFTypeRef(),
                                        n1.as_CFTypeRef(),
                                        n2.as_CFTypeRef(),
                                        n3.as_CFTypeRef(),
                                        n4.as_CFTypeRef(),
                                        n5.as_CFTypeRef()]);

        let mut sum = 0;

        let mut iter = arr.iter();
        assert_eq!(iter.len(), 6);
        assert!(iter.next().is_some());
        assert_eq!(iter.len(), 5);

        for elem in iter {
            let number: CFNumber = elem.downcast::<CFNumber>().unwrap();
            sum += number.to_i64().unwrap()
        }

        assert!(sum == 15);

        for elem in arr.iter() {
            let number: CFNumber = elem.downcast::<CFNumber>().unwrap();
            sum += number.to_i64().unwrap()
        }

        assert!(sum == 30);
    }
}
