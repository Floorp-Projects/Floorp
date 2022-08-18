/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::interfaces::{nsISupports, nsrefcnt};
use libc;
use nserror::{nsresult, NS_OK};
use std::cell::Cell;
use std::fmt;
use std::marker::PhantomData;
use std::mem;
use std::ops::Deref;
use std::ptr::{self, NonNull};
use std::sync::atomic::{self, AtomicUsize, Ordering};
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
#[repr(transparent)]
pub struct RefPtr<T: RefCounted + 'static> {
    _ptr: NonNull<T>,
    // Tell dropck that we own an instance of T.
    _marker: PhantomData<T>,
}

impl<T: RefCounted + 'static> RefPtr<T> {
    /// Construct a new RefPtr from a reference to the refcounted object.
    #[inline]
    pub fn new(p: &T) -> RefPtr<T> {
        unsafe {
            p.addref();
        }
        RefPtr {
            _ptr: p.into(),
            _marker: PhantomData,
        }
    }

    /// Construct a RefPtr from a raw pointer, addrefing it.
    #[inline]
    pub unsafe fn from_raw(p: *const T) -> Option<RefPtr<T>> {
        let ptr = NonNull::new(p as *mut T)?;
        ptr.as_ref().addref();
        Some(RefPtr {
            _ptr: ptr,
            _marker: PhantomData,
        })
    }

    /// Construct a RefPtr from a raw pointer, without addrefing it.
    #[inline]
    pub unsafe fn from_raw_dont_addref(p: *const T) -> Option<RefPtr<T>> {
        Some(RefPtr {
            _ptr: NonNull::new(p as *mut T)?,
            _marker: PhantomData,
        })
    }

    /// Write this RefPtr's value into an outparameter.
    #[inline]
    pub fn forget(self, into: &mut *const T) {
        *into = Self::forget_into_raw(self);
    }

    #[inline]
    pub fn forget_into_raw(this: RefPtr<T>) -> *const T {
        let into = &*this as *const T;
        mem::forget(this);
        into
    }
}

impl<T: RefCounted + 'static> Deref for RefPtr<T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        unsafe { self._ptr.as_ref() }
    }
}

impl<T: RefCounted + 'static> Drop for RefPtr<T> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            self._ptr.as_ref().release();
        }
    }
}

impl<T: RefCounted + 'static> Clone for RefPtr<T> {
    #[inline]
    fn clone(&self) -> RefPtr<T> {
        RefPtr::new(self)
    }
}

impl<T: RefCounted + 'static + fmt::Debug> fmt::Debug for RefPtr<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "RefPtr<{:?}>", self.deref())
    }
}

// Both `Send` and `Sync` bounds are required for `RefPtr<T>` to implement
// either, as sharing a `RefPtr<T>` also allows transferring ownership, and
// vice-versa.
unsafe impl<T: RefCounted + 'static + Send + Sync> Send for RefPtr<T> {}
unsafe impl<T: RefCounted + 'static + Send + Sync> Sync for RefPtr<T> {}

macro_rules! assert_layout_eq {
    ($T:ty, $U:ty) => {
        const _: [(); std::mem::size_of::<$T>()] = [(); std::mem::size_of::<$U>()];
        const _: [(); std::mem::align_of::<$T>()] = [(); std::mem::align_of::<$U>()];
    };
}

// Assert that `RefPtr<nsISupports>` has the correct memory layout.
assert_layout_eq!(RefPtr<nsISupports>, *const nsISupports);
// Assert that the null-pointer optimization applies to `RefPtr<nsISupports>`.
assert_layout_eq!(RefPtr<nsISupports>, Option<RefPtr<nsISupports>>);

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

impl<T: RefCounted + 'static> GetterAddrefs<T> {
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
        unsafe { RefPtr::from_raw_dont_addref(p) }
    }
}

impl<T: RefCounted + 'static> Drop for GetterAddrefs<T> {
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
where
    F: FnOnce(*mut *const T) -> nsresult,
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
/// `#[xpcom(nonatomic)]` will use this type for the `__refcnt` field.
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
/// `#[xpcom(atomic)]` will use this type for the `__refcnt` field.
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
            // We're going to destroy the object on this thread, so we need
            // acquire semantics to synchronize with the memory released by
            // the last release on other threads, that is, to ensure that
            // writes prior to that release are now visible on this thread.
            if cfg!(feature = "thread_sanitizer") {
                // TSan doesn't understand atomic::fence, so in order to avoid
                // a false positive for every time a refcounted object is
                // deleted, we replace the fence with an atomic operation.
                self.0.load(Ordering::Acquire);
            } else {
                atomic::fence(Ordering::Acquire);
            }
        }
        result
    }

    /// Get the current value of the reference count.
    pub fn get(&self) -> nsrefcnt {
        self.0.load(Ordering::Acquire) as nsrefcnt
    }
}

#[cfg(feature = "gecko_refcount_logging")]
pub mod trace_refcnt {
    use crate::interfaces::nsrefcnt;

    extern "C" {
        pub fn NS_LogCtor(aPtr: *mut libc::c_void, aTypeName: *const libc::c_char, aSize: u32);
        pub fn NS_LogDtor(aPtr: *mut libc::c_void, aTypeName: *const libc::c_char, aSize: u32);
        pub fn NS_LogAddRef(
            aPtr: *mut libc::c_void,
            aRefcnt: nsrefcnt,
            aClass: *const libc::c_char,
            aClassSize: u32,
        );
        pub fn NS_LogRelease(
            aPtr: *mut libc::c_void,
            aRefcnt: nsrefcnt,
            aClass: *const libc::c_char,
            aClassSize: u32,
        );
    }
}

// stub inline methods for the refcount logging functions for when the feature
// is disabled.
#[cfg(not(feature = "gecko_refcount_logging"))]
pub mod trace_refcnt {
    use crate::interfaces::nsrefcnt;

    #[inline]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn NS_LogCtor(_: *mut libc::c_void, _: *const libc::c_char, _: u32) {}
    #[inline]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn NS_LogDtor(_: *mut libc::c_void, _: *const libc::c_char, _: u32) {}
    #[inline]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn NS_LogAddRef(
        _: *mut libc::c_void,
        _: nsrefcnt,
        _: *const libc::c_char,
        _: u32,
    ) {
    }
    #[inline]
    #[allow(non_snake_case)]
    pub unsafe extern "C" fn NS_LogRelease(
        _: *mut libc::c_void,
        _: nsrefcnt,
        _: *const libc::c_char,
        _: u32,
    ) {
    }
}
