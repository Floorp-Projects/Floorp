/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! An owned slice that tries to use only one word of storage.
//!
//! `ThinBoxedSlice<T>` can be used in place of `Box<[T]>` on the `x86_64`
//! architecture to hold ownership of a slice when it's important to reduce
//! memory usage of the box itself. When the slice length is less than
//! `0xffff`, a single word is used to encode the slice pointer and length.
//! When it is greater than `0xffff`, a heap allocation is used to store the
//! fat pointer representing the slice.
//!
//! A `ThinBoxedSlice<T>` is always created by converting from a `Box<[T]>`.
//!
//! On any architecture other than `x86_64`, a `ThinBoxedSlice<T>` will simply
//! use a `Box<[T]>` internally.
//!
//! # Examples
//!
//! Creating a `ThinBoxedSlice`:
//!
//! ```
//! # use thin_slice::ThinBoxedSlice;
//! let fat_pointer = vec![10, 20, 30].into_boxed_slice();
//! let thin_pointer: ThinBoxedSlice<_> = fat_pointer.into();
//! ```

use std::cmp::Ordering;
use std::fmt;
use std::hash::{Hash, Hasher};
#[cfg(target_arch = "x86_64")]
use std::marker::PhantomData;
#[cfg(target_arch = "x86_64")]
use std::mem;
use std::ops::{Deref, DerefMut};
#[cfg(target_arch = "x86_64")]
use std::ptr::NonNull;
#[cfg(target_arch = "x86_64")]
use std::slice;

/// An owned slice that tries to use only one word of storage.
///
/// See the [module-level documentation](index.html) for more.
pub struct ThinBoxedSlice<T> {
    /// Storage for the slice.
    ///
    /// Once `std::num::NonZeroUsize` has stabilized, we can switch to that.
    ///
    /// The value stored here depends on the length of the slice.
    ///
    /// If len = 0, `data` will be `1usize`.
    ///
    /// If 0 < len < 0xffff, then len will be stored in the top 16 bits of
    /// data, and the lower 48 bits will be the pointer to the elements.
    ///
    /// If len >= 0xffff, then the top 16 bits of data will be 0xffff, and
    /// the lower 48 bits will be a pointer to a heap allocated `Box<[T]>`.
    #[cfg(target_arch = "x86_64")]
    data: NonNull<()>,

    #[cfg(not(target_arch = "x86_64"))]
    data: Box<[T]>,

    #[cfg(target_arch = "x86_64")]
    _phantom: PhantomData<Box<[T]>>,
}

#[cfg(target_arch = "x86_64")]
const TAG_MASK: usize = 0xffff000000000000;

#[cfg(target_arch = "x86_64")]
const PTR_MASK: usize = 0x0000ffffffffffff;

#[cfg(target_arch = "x86_64")]
const PTR_HIGH: usize = 0x0000800000000000;

#[cfg(target_arch = "x86_64")]
const TAG_SHIFT: usize = 48;

#[cfg(target_arch = "x86_64")]
const TAG_LIMIT: usize = TAG_MASK >> TAG_SHIFT;

#[cfg(target_arch = "x86_64")]
enum Storage<T> {
    Inline(*mut T, usize),
    Spilled(*mut Box<[T]>),
}

#[cfg(target_arch = "x86_64")]
impl<T> ThinBoxedSlice<T> {
    /// Constructs a `ThinBoxedSlice` from a raw pointer.
    ///
    /// Like `Box::from_raw`, after calling this function, the raw pointer is
    /// owned by the resulting `ThinBoxedSlice`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// let x = vec![10, 20, 30].into_boxed_slice();       // a Box<[i32]>
    /// let ptr = Box::into_raw(x);                        // a *mut [i32]
    /// let x = unsafe { ThinBoxedSlice::from_raw(ptr) };  // a ThinBoxedSlice<i32>
    /// ```
    #[inline]
    pub unsafe fn from_raw(raw: *mut [T]) -> ThinBoxedSlice<T> {
        let len = (*raw).len();
        let ptr = (*raw).as_mut_ptr();
        let storage = if len == 0 {
            Storage::Inline(1usize as *mut _, 0)
        } else if len < TAG_LIMIT {
            Storage::Inline(ptr, len)
        } else {
            let boxed_slice = Box::from_raw(raw);
            Storage::Spilled(Box::into_raw(Box::new(boxed_slice)))
        };
        ThinBoxedSlice {
            data: storage.into_data(),
            _phantom: PhantomData,
        }
    }

    /// Consumes the `ThinBoxedSlice`, returning a raw pointer to the slice
    /// it owned.
    ///
    /// Like `Box::into_raw`, after calling this function, the caller is
    /// responsible for the memory previously managed by the `ThinBoxedSlice`.
    /// In particular, the caller should properly destroy the `[T]` and release
    /// the memory. The proper way to do so is to convert the raw pointer back
    /// into a `Box` or a `ThinBoxedSlice`, with either the `Box::from_raw` or
    /// `ThinBoxedSlice::from_raw` functions.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// let x = vec![10, 20, 30].into_boxed_slice();
    /// let x = ThinBoxedSlice::from(x);
    /// let ptr = ThinBoxedSlice::into_raw(x);
    /// ```
    #[inline]
    pub fn into_raw(b: ThinBoxedSlice<T>) -> *mut [T] {
        unsafe {
            match b.into_storage() {
                Storage::Inline(ptr, len) => {
                    slice::from_raw_parts_mut(ptr, len)
                }
                Storage::Spilled(ptr) => {
                    Box::into_raw(*Box::from_raw(ptr))
                }
            }
        }
    }

    /// Consumes and leaks the `ThinBoxedSlice`, returning a mutable reference,
    /// `&'a mut [T]`. Here, the lifetime `'a` may be chosen to be `'static`.
    ///
    /// Like `Box::leak`, this function is mainly useful for data that lives
    /// for the remainder of the program's life. Dropping the returned
    /// reference will cause a memory leak. If this is not acceptable, the
    /// reference should first be wrapped with the `Box::from_raw` function
    /// producing a `Box`, or with the `ThinBoxedSlice::from_raw` function
    /// producing a `ThinBoxedSlice`. This value can then be dropped which will
    /// properly destroy `[T]` and release the allocated memory.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// fn main() {
    ///     let x = ThinBoxedSlice::from(vec![1, 2, 3].into_boxed_slice());
    ///     let static_ref = ThinBoxedSlice::leak(x);
    ///     static_ref[0] = 4;
    ///     assert_eq!(*static_ref, [4, 2, 3]);
    /// }
    /// ```
    #[inline]
    pub fn leak(b: ThinBoxedSlice<T>) -> &'static mut [T] {
        unsafe { &mut *ThinBoxedSlice::into_raw(b) }
    }

    /// Returns a pointer to the heap allocation that stores the fat pointer
    /// to the slice, if any. This is useful for systems that need to measure
    /// memory allocation, but is otherwise an opaque pointer.
    #[inline]
    pub fn spilled_storage(&self) -> Option<*const ()> {
        match self.storage() {
            Storage::Inline(..) => None,
            Storage::Spilled(ptr) => Some(ptr as *const ()),
        }
    }

    #[inline]
    fn storage(&self) -> Storage<T> {
        Storage::from_data(self.data.clone())
    }

    #[inline]
    fn into_storage(self) -> Storage<T> {
        let storage = self.storage();
        mem::forget(self);
        storage
    }
}

#[cfg(not(target_arch = "x86_64"))]
impl<T> ThinBoxedSlice<T> {
    /// Constructs a `ThinBoxedSlice` from a raw pointer.
    ///
    /// Like `Box::from_raw`, after calling this function, the raw pointer is
    /// owned by the resulting `ThinBoxedSlice`.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// let x = vec![10, 20, 30].into_boxed_slice();       // a Box<[i32]>
    /// let ptr = Box::into_raw(x);                        // a *mut [i32]
    /// let x = unsafe { ThinBoxedSlice::from_raw(ptr) };  // a ThinBoxedSlice<i32>
    /// ```
    #[inline]
    pub unsafe fn from_raw(raw: *mut [T]) -> ThinBoxedSlice<T> {
        ThinBoxedSlice {
            data: Box::from_raw(raw),
        }
    }

    /// Consumes the `ThinBoxedSlice`, returning a raw pointer to the slice
    /// it owned.
    ///
    /// Like `Box::into_raw`, after calling this function, the caller is
    /// responsible for the memory previously managed by the `ThinBoxedSlice`.
    /// In particular, the caller should properly destroy the `[T]` and release
    /// the memory. The proper way to do so is to convert the raw pointer back
    /// into a `Box` or a `ThinBoxedSlice`, with either the `Box::from_raw` or
    /// `ThinBoxedSlice::from_raw` functions.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// let x = vec![10, 20, 30].into_boxed_slice();
    /// let x = ThinBoxedSlice::from(x);
    /// let ptr = ThinBoxedSlice::into_raw(x);
    /// ```
    #[inline]
    pub fn into_raw(b: ThinBoxedSlice<T>) -> *mut [T] {
        Box::into_raw(b.data)
    }

    /// Consumes and leaks the `ThinBoxedSlice`, returning a mutable reference,
    /// `&'a mut [T]`. Here, the lifetime `'a` may be chosen to be `'static`.
    ///
    /// Like `Box::leak`, this function is mainly useful for data that lives
    /// for the remainder of the program's life. Dropping the returned
    /// reference will cause a memory leak. If this is not acceptable, the
    /// reference should first be wrapped with the `Box::from_raw` function
    /// producing a `Box`, or with the `ThinBoxedSlice::from_raw` function
    /// producing a `ThinBoxedSlice`. This value can then be dropped which will
    /// properly destroy `[T]` and release the allocated memory.
    ///
    /// # Examples
    ///
    /// ```
    /// # use thin_slice::ThinBoxedSlice;
    /// fn main() {
    ///     let x = ThinBoxedSlice::from(vec![1, 2, 3].into_boxed_slice());
    ///     let static_ref = ThinBoxedSlice::leak(x);
    ///     static_ref[0] = 4;
    ///     assert_eq!(*static_ref, [4, 2, 3]);
    /// }
    /// ```
    #[inline]
    pub fn leak<'a>(b: ThinBoxedSlice<T>) -> &'a mut [T] where T: 'a {
        Box::leak(b.data)
    }

    /// Returns a pointer to the heap allocation that stores the fat pointer
    /// to the slice, if any. This is useful for systems that need to measure
    /// memory allocation, but is otherwise an opaque pointer.
    #[inline]
    pub fn spilled_storage(&self) -> Option<*const ()> {
        None
    }
}

#[cfg(target_arch = "x86_64")]
impl<T> Storage<T> {
    #[inline]
    fn from_data(data: NonNull<()>) -> Storage<T> {
        let data = data.as_ptr() as usize;

        let len = (data & TAG_MASK) >> TAG_SHIFT;
        let mut ptr = data & PTR_MASK;

        if (ptr & PTR_HIGH) == PTR_HIGH {
            // Canonical linear addresses on x86_64 are sign extended from
            // bit 48.
            ptr |= TAG_MASK;
        }

        if len < TAG_LIMIT {
            Storage::Inline(ptr as *mut T, len)
        } else {
            Storage::Spilled(ptr as *mut Box<[T]>)
        }
    }

    #[inline]
    fn into_data(self) -> NonNull<()> {
        let data = match self {
            Storage::Inline(ptr, len) => {
                (len << TAG_SHIFT) | ((ptr as usize) & PTR_MASK)
            }
            Storage::Spilled(ptr) => {
                TAG_MASK | ((ptr as usize) & PTR_MASK)
            }
        };
        unsafe {
            NonNull::new_unchecked(data as *mut _)
        }
    }
}

impl<T> From<Box<[T]>> for ThinBoxedSlice<T> {
    fn from(value: Box<[T]>) -> ThinBoxedSlice<T> {
        let ptr = Box::into_raw(value);
        unsafe {
            ThinBoxedSlice::from_raw(ptr)
        }
    }
}

impl<T> Into<Box<[T]>> for ThinBoxedSlice<T> {
    fn into(self) -> Box<[T]> {
        let ptr = ThinBoxedSlice::into_raw(self);
        unsafe {
            Box::from_raw(ptr)
        }
    }
}

unsafe impl<T: Send> Send for ThinBoxedSlice<T> {}
unsafe impl<T: Sync> Sync for ThinBoxedSlice<T> {}

#[cfg(target_arch = "x86_64")]
impl<T> Drop for ThinBoxedSlice<T> {
    fn drop(&mut self) {
        let _ = Into::<Box<[T]>>::into(
            ThinBoxedSlice {
                data: self.data.clone(),
                _phantom: PhantomData,
            }
        );
    }
}

impl<T: Clone> Clone for ThinBoxedSlice<T> {
    #[cfg(target_arch = "x86_64")]
    fn clone(&self) -> Self {
        unsafe {
            match self.storage() {
                Storage::Inline(ptr, len) => {
                    slice::from_raw_parts_mut(ptr, len)
                        .to_vec()
                        .into_boxed_slice()
                        .into()
                }
                Storage::Spilled(ptr) => {
                    (*ptr).clone().into()
                }
            }
        }
    }

    #[cfg(not(target_arch = "x86_64"))]
    fn clone(&self) -> Self {
        ThinBoxedSlice {
            data: self.data.clone(),
        }
    }
}

impl<T> AsRef<[T]> for ThinBoxedSlice<T> {
    fn as_ref(&self) -> &[T] {
        &**self
    }
}

impl<T> AsMut<[T]> for ThinBoxedSlice<T> {
    fn as_mut(&mut self) -> &mut [T] {
        &mut **self
    }
}

impl<T> Deref for ThinBoxedSlice<T> {
    type Target = [T];

    #[cfg(target_arch = "x86_64")]
    fn deref(&self) -> &[T] {
        unsafe {
            match self.storage() {
                Storage::Inline(ptr, len) => {
                    slice::from_raw_parts(ptr, len)
                }
                Storage::Spilled(ptr) => {
                    &**ptr
                }
            }
        }
    }

    #[cfg(not(target_arch = "x86_64"))]
    fn deref(&self) -> &[T] {
        &*self.data
    }
}

impl<T> DerefMut for ThinBoxedSlice<T> {
    #[cfg(target_arch = "x86_64")]
    fn deref_mut(&mut self) -> &mut [T] {
        unsafe {
            match self.storage() {
                Storage::Inline(ptr, len) => {
                    slice::from_raw_parts_mut(ptr, len)
                }
                Storage::Spilled(ptr) => {
                    &mut **ptr
                }
            }
        }
    }

    #[cfg(not(target_arch = "x86_64"))]
    fn deref_mut(&mut self) -> &mut [T] {
        &mut *self.data
    }
}

impl<T> Default for ThinBoxedSlice<T> {
    fn default() -> Self {
        Box::<[T]>::default().into()
    }
}

impl<T: fmt::Debug> fmt::Debug for ThinBoxedSlice<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&**self, f)
    }
}

impl<T: Eq> Eq for ThinBoxedSlice<T> {}

impl<T: Hash> Hash for ThinBoxedSlice<T> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        (**self).hash(state);
    }
}

impl<T: PartialEq> PartialEq for ThinBoxedSlice<T> {
    #[inline]
    fn eq(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialEq::eq(&**self, &**other)
    }
    #[inline]
    fn ne(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialEq::ne(&**self, &**other)
    }
}

impl<T: PartialOrd> PartialOrd for ThinBoxedSlice<T> {
    #[inline]
    fn partial_cmp(&self, other: &ThinBoxedSlice<T>) -> Option<Ordering> {
        PartialOrd::partial_cmp(&**self, &**other)
    }
    #[inline]
    fn lt(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialOrd::lt(&**self, &**other)
    }
    #[inline]
    fn le(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialOrd::le(&**self, &**other)
    }
    #[inline]
    fn ge(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialOrd::ge(&**self, &**other)
    }
    #[inline]
    fn gt(&self, other: &ThinBoxedSlice<T>) -> bool {
        PartialOrd::gt(&**self, &**other)
    }
}

impl<T> fmt::Pointer for ThinBoxedSlice<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let ptr = &**self;
        fmt::Pointer::fmt(&ptr, f)
    }
}

#[cfg(target_arch = "x86_64")]
#[test]
fn test_spilled_storage() {
    let x = ThinBoxedSlice::from(vec![0; TAG_LIMIT - 1].into_boxed_slice());
    assert!(x.spilled_storage().is_none());

    let x = ThinBoxedSlice::from(vec![0; TAG_LIMIT].into_boxed_slice());
    assert!(x.spilled_storage().is_some());
}

#[cfg(target_arch = "x86_64")]
#[test]
fn test_from_raw_large() {
    let mut vec = vec![0; TAG_LIMIT];
    vec[123] = 456;

    let ptr = Box::into_raw(vec.into_boxed_slice());
    let x = unsafe { ThinBoxedSlice::from_raw(ptr) };
    assert_eq!(x[123], 456);
}

#[cfg(target_arch = "x86_64")]
#[test]
fn test_into_raw_large() {
    let mut vec = vec![0; TAG_LIMIT];
    vec[123] = 456;

    let x = ThinBoxedSlice::from(vec.into_boxed_slice());
    let ptr = ThinBoxedSlice::into_raw(x);
    let y = unsafe { Box::from_raw(ptr) };
    assert_eq!(y[123], 456);
}

#[cfg(target_arch = "x86_64")]
#[test]
fn test_leak_large() {
    let mut vec = vec![0; TAG_LIMIT];
    vec[123] = 456;

    let x = ThinBoxedSlice::from(vec.into_boxed_slice());
    let static_ref = ThinBoxedSlice::leak(x);
    static_ref[123] *= 1000;
    assert_eq!(static_ref[123], 456000);
}
