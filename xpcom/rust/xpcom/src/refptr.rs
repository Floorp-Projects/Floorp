/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::mem;
use std::ptr;
use std::ops::Deref;
use std::fmt;
use std::marker::PhantomData;
use std::cell::Cell;
use std::sync::atomic::{self, AtomicUsize, Ordering};

use nserror::{nsresult, NS_OK};

use libc;

use interfaces::nsrefcnt;

use threadbound::ThreadBound;

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
    pub fn forget(self, into: &mut *const T) {
        *into = &*self;
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

impl <T: RefCounted + 'static + fmt::Debug> fmt::Debug for RefPtr<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RefPtr<{:?}>", self.deref())
    }
}

/// A wrapper that binds a RefCounted value to its original thread,
/// preventing retrieval from other threads and panicking if the value
/// is dropped on a different thread.
///
/// These limitations enable values of this type to be Send + Sync, which is
/// useful when creating a struct that holds a RefPtr<T> type while being
/// Send + Sync.  Such a struct can hold a ThreadBoundRefPtr<T> type instead.
pub struct ThreadBoundRefPtr<T: RefCounted + 'static>(ThreadBound<*const T>);

impl<T: RefCounted + 'static> ThreadBoundRefPtr<T> {
    pub fn new(ptr: RefPtr<T>) -> Self {
        let raw: *const T = &*ptr;
        mem::forget(ptr);
        ThreadBoundRefPtr(ThreadBound::new(raw))
    }

    pub fn get_ref(&self) -> Option<&T> {
        self.0.get_ref().map(|raw| unsafe { &**raw })
    }
}

impl<T: RefCounted + 'static> Drop for ThreadBoundRefPtr<T> {
    fn drop(&mut self) {
        unsafe {
            RefPtr::from_raw_dont_addref(self.get_ref().expect("drop() called on wrong thread!"));
        }
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

/// The type of the reference count type for xpcom structs.
///
/// `#[derive(xpcom)]` will use this type for the `__refcnt` field when
/// `#[refcnt = "nonatomic"]` is used.
#[derive(Debug)]
pub struct Refcnt(Cell<nsrefcnt>);
impl Refcnt {
    /// Create a new reference count value. This is unsafe as manipulating
    /// Refcnt values is an easy footgun.
    pub unsafe fn new() -> Self {
        Refcnt(Cell::new(0))
    }

    /// Increment the reference count. Returns the new reference count. This is
    /// unsafe as modifying this value can cause a use-after-free.
    pub unsafe fn inc(&self) -> nsrefcnt {
        // XXX: Checked add?
        let new = self.0.get() + 1;
        self.0.set(new);
        new
    }

    /// Decrement the reference count. Returns the new reference count. This is
    /// unsafe as modifying this value can cause a use-after-free.
    pub unsafe fn dec(&self) -> nsrefcnt {
        // XXX: Checked sub?
        let new = self.0.get() - 1;
        self.0.set(new);
        new
    }

    /// Get the current value of the reference count.
    pub fn get(&self) -> nsrefcnt {
        self.0.get()
    }
}

/// The type of the atomic reference count used for xpcom structs.
///
/// `#[derive(xpcom)]` will use this type for the `__refcnt` field when
/// `#[refcnt = "atomic"]` is used.
///
/// See `nsISupportsImpl.h`'s `ThreadSafeAutoRefCnt` class for reasoning behind
/// memory ordering decisions.
#[derive(Debug)]
pub struct AtomicRefcnt(AtomicUsize);
impl AtomicRefcnt {
    /// Create a new reference count value. This is unsafe as manipulating
    /// Refcnt values is an easy footgun.
    pub unsafe fn new() -> Self {
        AtomicRefcnt(AtomicUsize::new(0))
    }

    /// Increment the reference count. Returns the new reference count. This is
    /// unsafe as modifying this value can cause a use-after-free.
    pub unsafe fn inc(&self) -> nsrefcnt {
        self.0.fetch_add(1, Ordering::Relaxed) as nsrefcnt + 1
    }

    /// Decrement the reference count. Returns the new reference count. This is
    /// unsafe as modifying this value can cause a use-after-free.
    pub unsafe fn dec(&self) -> nsrefcnt {
        let result = self.0.fetch_sub(1, Ordering::Release) as nsrefcnt - 1;
        if result == 0 {
            // We're going to destroy the object on this thread.
            atomic::fence(Ordering::Acquire);
        }
        result
    }

    /// Get the current value of the reference count.
    pub fn get(&self) -> nsrefcnt {
        self.0.load(Ordering::Acquire) as nsrefcnt
    }
}
