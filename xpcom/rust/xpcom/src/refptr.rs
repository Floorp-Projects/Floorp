/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::mem;
use std::ptr;
use std::ops::Deref;
use std::marker::PhantomData;
use std::cell::Cell;
use std::sync::atomic::{self, AtomicUsize, Ordering};

use nserror::{NsresultExt, nsresult, NS_OK};

use libc;

/// A trait representing a type which can be reference counted invasively.
/// The object is responsible for freeing its backing memory when its
/// reference count reaches 0.
pub unsafe trait RefCounted {
    /// Increment the reference count.
    unsafe fn addref(&self);
    /// Decrement the reference count, potentially freeing backing memory.
    unsafe fn release(&self);
}

/// A smart pointer holding a RefCounted object. The object itself manages its
/// own memory. RefPtr will invoke the addref and release methods at the
/// appropriate times to facilitate the bookkeeping.
pub struct RefPtr<T: RefCounted + 'static> {
    // We're going to cheat and store the internal reference as an &'static T
    // instead of an *const T or Shared<T>, because Shared and NonZero are
    // unstable, and we need to build on stable rust.
    // I believe that this is "safe enough", as this module is private and
    // no other module can read this reference.
    _ptr: &'static T,
    // As we aren't using Shared<T>, we need to add this phantomdata to
    // prevent unsoundness in dropck
    _marker: PhantomData<T>,
}

impl <T: RefCounted + 'static> RefPtr<T> {
    /// Construct a new RefPtr from a reference to the refcounted object.
    #[inline]
    pub fn new(p: &T) -> RefPtr<T> {
        unsafe {
            p.addref();
            RefPtr {
                _ptr: mem::transmute(p),
                _marker: PhantomData,
            }
        }
    }

    /// Construct a RefPtr from a raw pointer, addrefing it.
    #[inline]
    pub unsafe fn from_raw(p: *const T) -> Option<RefPtr<T>> {
        if p.is_null() {
            return None;
        }
        (*p).addref();
        Some(RefPtr {
            _ptr: &*p,
            _marker: PhantomData,
        })
    }

    /// Construct a RefPtr from a raw pointer, without addrefing it.
    #[inline]
    pub unsafe fn from_raw_dont_addref(p: *const T) -> Option<RefPtr<T>> {
        if p.is_null() {
            return None;
        }
        Some(RefPtr {
            _ptr: &*p,
            _marker: PhantomData,
        })
    }

    /// Write this RefPtr's value into an outparameter.
    #[inline]
    pub unsafe fn forget(self, into: &mut *const T) {
        *into = &*self as *const T;
        mem::forget(self);
    }
}

impl <T: RefCounted + 'static> Deref for RefPtr<T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        self._ptr
    }
}

impl <T: RefCounted + 'static> Drop for RefPtr<T> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            self._ptr.release();
        }
    }
}

impl <T: RefCounted + 'static> Clone for RefPtr<T> {
    #[inline]
    fn clone(&self) -> RefPtr<T> {
        RefPtr::new(self)
    }
}

/// A helper struct for constructing `RefPtr<T>` from raw pointer outparameters.
/// Holds a `*const T` internally which will be released if non null when
/// destructed, and can be easily transformed into an `Option<RefPtr<T>>`.
///
/// It many cases it may be easier to use the `getter_addrefs` method.
pub struct GetterAddrefs<T: RefCounted + 'static> {
    _ptr: *const T,
    _marker: PhantomData<T>,
}

impl <T: RefCounted + 'static> GetterAddrefs<T> {
    /// Create a `GetterAddrefs`, initializing it with the null pointer.
    #[inline]
    pub fn new() -> GetterAddrefs<T> {
        GetterAddrefs {
            _ptr: ptr::null(),
            _marker: PhantomData,
        }
    }

    /// Get a reference to the internal `*const T`. This method is unsafe,
    /// as the destructor of this class depends on the internal `*const T`
    /// being either a valid reference to a value of type `T`, or null.
    #[inline]
    pub unsafe fn ptr(&mut self) -> &mut *const T {
        &mut self._ptr
    }

    /// Get a reference to the internal `*const T` as a `*mut libc::c_void`.
    /// This is useful to pass to functions like `GetInterface` which take a
    /// void pointer outparameter.
    #[inline]
    pub unsafe fn void_ptr(&mut self) -> *mut *mut libc::c_void {
        &mut self._ptr as *mut *const T as *mut *mut libc::c_void
    }

    /// Transform this `GetterAddrefs` into an `Option<RefPtr<T>>`, without
    /// performing any addrefs or releases.
    #[inline]
    pub fn refptr(self) -> Option<RefPtr<T>> {
        let p = self._ptr;
        // Don't run the destructor because we don't want to release the stored
        // pointer.
        mem::forget(self);
        unsafe {
            RefPtr::from_raw_dont_addref(p)
        }
    }
}

impl <T: RefCounted + 'static> Drop for GetterAddrefs<T> {
    #[inline]
    fn drop(&mut self) {
        if !self._ptr.is_null() {
            unsafe {
                (*self._ptr).release();
            }
        }
    }
}

/// Helper method for calling XPCOM methods which return a reference counted
/// value through an outparameter. Takes a lambda, which is called with a valid
/// outparameter argument (`*mut *const T`), and returns a `nsresult`. Returns
/// either a `RefPtr<T>` with the value returned from the outparameter, or a
/// `nsresult`.
///
/// # NOTE:
///
/// Can return `Err(NS_OK)` if the call succeeded, but the outparameter was set
/// to NULL.
///
/// # Usage
///
/// ```
/// let x: Result<RefPtr<T>, nsresult> =
///     getter_addrefs(|p| iosvc.NewURI(uri, ptr::null(), ptr::null(), p));
/// ```
#[inline]
pub fn getter_addrefs<T: RefCounted, F>(f: F) -> Result<RefPtr<T>, nsresult>
    where F: FnOnce(*mut *const T) -> nsresult
{
    let mut ga = GetterAddrefs::<T>::new();
    let rv = f(unsafe { ga.ptr() });
    if rv.failed() {
        return Err(rv);
    }
    ga.refptr().ok_or(NS_OK)
}
