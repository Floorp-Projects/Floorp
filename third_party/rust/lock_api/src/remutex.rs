// Copyright 2018 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use core::cell::{Cell, UnsafeCell};
use core::fmt;
use core::marker::PhantomData;
use core::mem;
use core::ops::Deref;
use core::sync::atomic::{AtomicUsize, Ordering};
use mutex::{RawMutex, RawMutexFair, RawMutexTimed};
use GuardNoSend;

#[cfg(feature = "owning_ref")]
use owning_ref::StableAddress;

/// Helper trait which returns a non-zero thread ID.
///
/// The simplest way to implement this trait is to return the address of a
/// thread-local variable.
///
/// # Safety
///
/// Implementations of this trait must ensure that no two active threads share
/// the same thread ID. However the ID of a thread that has exited can be
/// re-used since that thread is no longer active.
pub unsafe trait GetThreadId {
    /// Initial value.
    const INIT: Self;

    /// Returns a non-zero thread ID which identifies the current thread of
    /// execution.
    fn nonzero_thread_id(&self) -> usize;
}

struct RawReentrantMutex<R: RawMutex, G: GetThreadId> {
    owner: AtomicUsize,
    lock_count: Cell<usize>,
    mutex: R,
    get_thread_id: G,
}

impl<R: RawMutex, G: GetThreadId> RawReentrantMutex<R, G> {
    #[inline]
    fn lock_internal<F: FnOnce() -> bool>(&self, try_lock: F) -> bool {
        let id = self.get_thread_id.nonzero_thread_id();
        if self.owner.load(Ordering::Relaxed) == id {
            self.lock_count.set(
                self.lock_count
                    .get()
                    .checked_add(1)
                    .expect("ReentrantMutex lock count overflow"),
            );
        } else {
            if !try_lock() {
                return false;
            }
            self.owner.store(id, Ordering::Relaxed);
            self.lock_count.set(1);
        }
        true
    }

    #[inline]
    fn lock(&self) {
        self.lock_internal(|| {
            self.mutex.lock();
            true
        });
    }

    #[inline]
    fn try_lock(&self) -> bool {
        self.lock_internal(|| self.mutex.try_lock())
    }

    #[inline]
    fn unlock(&self) {
        let lock_count = self.lock_count.get() - 1;
        if lock_count == 0 {
            self.owner.store(0, Ordering::Relaxed);
            self.mutex.unlock();
        } else {
            self.lock_count.set(lock_count);
        }
    }
}

impl<R: RawMutexFair, G: GetThreadId> RawReentrantMutex<R, G> {
    #[inline]
    fn unlock_fair(&self) {
        let lock_count = self.lock_count.get() - 1;
        if lock_count == 0 {
            self.owner.store(0, Ordering::Relaxed);
            self.mutex.unlock_fair();
        } else {
            self.lock_count.set(lock_count);
        }
    }

    #[inline]
    fn bump(&self) {
        if self.lock_count.get() == 1 {
            let id = self.owner.load(Ordering::Relaxed);
            self.owner.store(0, Ordering::Relaxed);
            self.mutex.bump();
            self.owner.store(id, Ordering::Relaxed);
        }
    }
}

impl<R: RawMutexTimed, G: GetThreadId> RawReentrantMutex<R, G> {
    #[inline]
    fn try_lock_until(&self, timeout: R::Instant) -> bool {
        self.lock_internal(|| self.mutex.try_lock_until(timeout))
    }

    #[inline]
    fn try_lock_for(&self, timeout: R::Duration) -> bool {
        self.lock_internal(|| self.mutex.try_lock_for(timeout))
    }
}

/// A mutex which can be recursively locked by a single thread.
///
/// This type is identical to `Mutex` except for the following points:
///
/// - Locking multiple times from the same thread will work correctly instead of
///   deadlocking.
/// - `ReentrantMutexGuard` does not give mutable references to the locked data.
///   Use a `RefCell` if you need this.
///
/// See [`Mutex`](struct.Mutex.html) for more details about the underlying mutex
/// primitive.
pub struct ReentrantMutex<R: RawMutex, G: GetThreadId, T: ?Sized> {
    raw: RawReentrantMutex<R, G>,
    data: UnsafeCell<T>,
}

unsafe impl<R: RawMutex + Send, G: GetThreadId + Send, T: ?Sized + Send> Send
    for ReentrantMutex<R, G, T>
{}
unsafe impl<R: RawMutex + Sync, G: GetThreadId + Sync, T: ?Sized + Send> Sync
    for ReentrantMutex<R, G, T>
{}

impl<R: RawMutex, G: GetThreadId, T> ReentrantMutex<R, G, T> {
    /// Creates a new reentrant mutex in an unlocked state ready for use.
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new(val: T) -> ReentrantMutex<R, G, T> {
        ReentrantMutex {
            data: UnsafeCell::new(val),
            raw: RawReentrantMutex {
                owner: AtomicUsize::new(0),
                lock_count: Cell::new(0),
                mutex: R::INIT,
                get_thread_id: G::INIT,
            },
        }
    }

    /// Creates a new reentrant mutex in an unlocked state ready for use.
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new(val: T) -> ReentrantMutex<R, G, T> {
        ReentrantMutex {
            data: UnsafeCell::new(val),
            raw: RawReentrantMutex {
                owner: AtomicUsize::new(0),
                lock_count: Cell::new(0),
                mutex: R::INIT,
                get_thread_id: G::INIT,
            },
        }
    }

    /// Consumes this mutex, returning the underlying data.
    #[inline]
    #[allow(unused_unsafe)]
    pub fn into_inner(self) -> T {
        unsafe { self.data.into_inner() }
    }
}

impl<R: RawMutex, G: GetThreadId, T: ?Sized> ReentrantMutex<R, G, T> {
    #[inline]
    fn guard(&self) -> ReentrantMutexGuard<R, G, T> {
        ReentrantMutexGuard {
            remutex: &self,
            marker: PhantomData,
        }
    }

    /// Acquires a reentrant mutex, blocking the current thread until it is able
    /// to do so.
    ///
    /// If the mutex is held by another thread then this function will block the
    /// local thread until it is available to acquire the mutex. If the mutex is
    /// already held by the current thread then this function will increment the
    /// lock reference count and return immediately. Upon returning,
    /// the thread is the only thread with the mutex held. An RAII guard is
    /// returned to allow scoped unlock of the lock. When the guard goes out of
    /// scope, the mutex will be unlocked.
    #[inline]
    pub fn lock(&self) -> ReentrantMutexGuard<R, G, T> {
        self.raw.lock();
        self.guard()
    }

    /// Attempts to acquire this lock.
    ///
    /// If the lock could not be acquired at this time, then `None` is returned.
    /// Otherwise, an RAII guard is returned. The lock will be unlocked when the
    /// guard is dropped.
    ///
    /// This function does not block.
    #[inline]
    pub fn try_lock(&self) -> Option<ReentrantMutexGuard<R, G, T>> {
        if self.raw.try_lock() {
            Some(self.guard())
        } else {
            None
        }
    }

    /// Returns a mutable reference to the underlying data.
    ///
    /// Since this call borrows the `ReentrantMutex` mutably, no actual locking needs to
    /// take place---the mutable borrow statically guarantees no locks exist.
    #[inline]
    pub fn get_mut(&mut self) -> &mut T {
        unsafe { &mut *self.data.get() }
    }

    /// Forcibly unlocks the mutex.
    ///
    /// This is useful when combined with `mem::forget` to hold a lock without
    /// the need to maintain a `ReentrantMutexGuard` object alive, for example when
    /// dealing with FFI.
    ///
    /// # Safety
    ///
    /// This method must only be called if the current thread logically owns a
    /// `ReentrantMutexGuard` but that guard has be discarded using `mem::forget`.
    /// Behavior is undefined if a mutex is unlocked when not locked.
    #[inline]
    pub unsafe fn force_unlock(&self) {
        self.raw.unlock();
    }

    /// Returns the underlying raw mutex object.
    ///
    /// Note that you will most likely need to import the `RawMutex` trait from
    /// `lock_api` to be able to call functions on the raw mutex.
    ///
    /// # Safety
    ///
    /// This method is unsafe because it allows unlocking a mutex while
    /// still holding a reference to a `ReentrantMutexGuard`.
    #[inline]
    pub unsafe fn raw(&self) -> &R {
        &self.raw.mutex
    }
}

impl<R: RawMutexFair, G: GetThreadId, T: ?Sized> ReentrantMutex<R, G, T> {
    /// Forcibly unlocks the mutex using a fair unlock procotol.
    ///
    /// This is useful when combined with `mem::forget` to hold a lock without
    /// the need to maintain a `ReentrantMutexGuard` object alive, for example when
    /// dealing with FFI.
    ///
    /// # Safety
    ///
    /// This method must only be called if the current thread logically owns a
    /// `ReentrantMutexGuard` but that guard has be discarded using `mem::forget`.
    /// Behavior is undefined if a mutex is unlocked when not locked.
    #[inline]
    pub unsafe fn force_unlock_fair(&self) {
        self.raw.unlock_fair();
    }
}

impl<R: RawMutexTimed, G: GetThreadId, T: ?Sized> ReentrantMutex<R, G, T> {
    /// Attempts to acquire this lock until a timeout is reached.
    ///
    /// If the lock could not be acquired before the timeout expired, then
    /// `None` is returned. Otherwise, an RAII guard is returned. The lock will
    /// be unlocked when the guard is dropped.
    #[inline]
    pub fn try_lock_for(&self, timeout: R::Duration) -> Option<ReentrantMutexGuard<R, G, T>> {
        if self.raw.try_lock_for(timeout) {
            Some(self.guard())
        } else {
            None
        }
    }

    /// Attempts to acquire this lock until a timeout is reached.
    ///
    /// If the lock could not be acquired before the timeout expired, then
    /// `None` is returned. Otherwise, an RAII guard is returned. The lock will
    /// be unlocked when the guard is dropped.
    #[inline]
    pub fn try_lock_until(&self, timeout: R::Instant) -> Option<ReentrantMutexGuard<R, G, T>> {
        if self.raw.try_lock_until(timeout) {
            Some(self.guard())
        } else {
            None
        }
    }
}

impl<R: RawMutex, G: GetThreadId, T: ?Sized + Default> Default for ReentrantMutex<R, G, T> {
    #[inline]
    fn default() -> ReentrantMutex<R, G, T> {
        ReentrantMutex::new(Default::default())
    }
}

impl<R: RawMutex, G: GetThreadId, T> From<T> for ReentrantMutex<R, G, T> {
    #[inline]
    fn from(t: T) -> ReentrantMutex<R, G, T> {
        ReentrantMutex::new(t)
    }
}

impl<R: RawMutex, G: GetThreadId, T: ?Sized + fmt::Debug> fmt::Debug for ReentrantMutex<R, G, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.try_lock() {
            Some(guard) => f
                .debug_struct("ReentrantMutex")
                .field("data", &&*guard)
                .finish(),
            None => f.pad("ReentrantMutex { <locked> }"),
        }
    }
}

/// An RAII implementation of a "scoped lock" of a reentrant mutex. When this structure
/// is dropped (falls out of scope), the lock will be unlocked.
///
/// The data protected by the mutex can be accessed through this guard via its
/// `Deref` implementation.
#[must_use]
pub struct ReentrantMutexGuard<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> {
    remutex: &'a ReentrantMutex<R, G, T>,
    marker: PhantomData<(&'a T, GuardNoSend)>,
}

unsafe impl<'a, R: RawMutex + Sync + 'a, G: GetThreadId + Sync + 'a, T: ?Sized + Sync + 'a> Sync
    for ReentrantMutexGuard<'a, R, G, T>
{}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> ReentrantMutexGuard<'a, R, G, T> {
    /// Returns a reference to the original `ReentrantMutex` object.
    pub fn remutex(s: &Self) -> &'a ReentrantMutex<R, G, T> {
        s.remutex
    }

    /// Makes a new `MappedReentrantMutexGuard` for a component of the locked data.
    ///
    /// This operation cannot fail as the `ReentrantMutexGuard` passed
    /// in already locked the mutex.
    ///
    /// This is an associated function that needs to be
    /// used as `ReentrantMutexGuard::map(...)`. A method would interfere with methods of
    /// the same name on the contents of the locked data.
    #[inline]
    pub fn map<U: ?Sized, F>(s: Self, f: F) -> MappedReentrantMutexGuard<'a, R, G, U>
    where
        F: FnOnce(&T) -> &U,
    {
        let raw = &s.remutex.raw;
        let data = f(unsafe { &*s.remutex.data.get() });
        mem::forget(s);
        MappedReentrantMutexGuard {
            raw,
            data,
            marker: PhantomData,
        }
    }

    /// Temporarily unlocks the mutex to execute the given function.
    ///
    /// This is safe because `&mut` guarantees that there exist no other
    /// references to the data protected by the mutex.
    #[inline]
    pub fn unlocked<F, U>(s: &mut Self, f: F) -> U
    where
        F: FnOnce() -> U,
    {
        s.remutex.raw.unlock();
        defer!(s.remutex.raw.lock());
        f()
    }
}

impl<'a, R: RawMutexFair + 'a, G: GetThreadId + 'a, T: ?Sized + 'a>
    ReentrantMutexGuard<'a, R, G, T>
{
    /// Unlocks the mutex using a fair unlock protocol.
    ///
    /// By default, mutexes are unfair and allow the current thread to re-lock
    /// the mutex before another has the chance to acquire the lock, even if
    /// that thread has been blocked on the mutex for a long time. This is the
    /// default because it allows much higher throughput as it avoids forcing a
    /// context switch on every mutex unlock. This can result in one thread
    /// acquiring a mutex many more times than other threads.
    ///
    /// However in some cases it can be beneficial to ensure fairness by forcing
    /// the lock to pass on to a waiting thread if there is one. This is done by
    /// using this method instead of dropping the `ReentrantMutexGuard` normally.
    #[inline]
    pub fn unlock_fair(s: Self) {
        s.remutex.raw.unlock_fair();
        mem::forget(s);
    }

    /// Temporarily unlocks the mutex to execute the given function.
    ///
    /// The mutex is unlocked a fair unlock protocol.
    ///
    /// This is safe because `&mut` guarantees that there exist no other
    /// references to the data protected by the mutex.
    #[inline]
    pub fn unlocked_fair<F, U>(s: &mut Self, f: F) -> U
    where
        F: FnOnce() -> U,
    {
        s.remutex.raw.unlock_fair();
        defer!(s.remutex.raw.lock());
        f()
    }

    /// Temporarily yields the mutex to a waiting thread if there is one.
    ///
    /// This method is functionally equivalent to calling `unlock_fair` followed
    /// by `lock`, however it can be much more efficient in the case where there
    /// are no waiting threads.
    #[inline]
    pub fn bump(s: &mut Self) {
        s.remutex.raw.bump();
    }
}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> Deref
    for ReentrantMutexGuard<'a, R, G, T>
{
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        unsafe { &*self.remutex.data.get() }
    }
}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> Drop
    for ReentrantMutexGuard<'a, R, G, T>
{
    #[inline]
    fn drop(&mut self) {
        self.remutex.raw.unlock();
    }
}

#[cfg(feature = "owning_ref")]
unsafe impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> StableAddress
    for ReentrantMutexGuard<'a, R, G, T>
{}

/// An RAII mutex guard returned by `ReentrantMutexGuard::map`, which can point to a
/// subfield of the protected data.
///
/// The main difference between `MappedReentrantMutexGuard` and `ReentrantMutexGuard` is that the
/// former doesn't support temporarily unlocking and re-locking, since that
/// could introduce soundness issues if the locked object is modified by another
/// thread.
#[must_use]
pub struct MappedReentrantMutexGuard<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> {
    raw: &'a RawReentrantMutex<R, G>,
    data: *const T,
    marker: PhantomData<&'a T>,
}

unsafe impl<'a, R: RawMutex + Sync + 'a, G: GetThreadId + Sync + 'a, T: ?Sized + Sync + 'a> Sync
    for MappedReentrantMutexGuard<'a, R, G, T>
{}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a>
    MappedReentrantMutexGuard<'a, R, G, T>
{
    /// Makes a new `MappedReentrantMutexGuard` for a component of the locked data.
    ///
    /// This operation cannot fail as the `ReentrantMutexGuard` passed
    /// in already locked the mutex.
    ///
    /// This is an associated function that needs to be
    /// used as `ReentrantMutexGuard::map(...)`. A method would interfere with methods of
    /// the same name on the contents of the locked data.
    #[inline]
    pub fn map<U: ?Sized, F>(s: Self, f: F) -> MappedReentrantMutexGuard<'a, R, G, U>
    where
        F: FnOnce(&T) -> &U,
    {
        let raw = s.raw;
        let data = f(unsafe { &*s.data });
        mem::forget(s);
        MappedReentrantMutexGuard {
            raw,
            data,
            marker: PhantomData,
        }
    }
}

impl<'a, R: RawMutexFair + 'a, G: GetThreadId + 'a, T: ?Sized + 'a>
    MappedReentrantMutexGuard<'a, R, G, T>
{
    /// Unlocks the mutex using a fair unlock protocol.
    ///
    /// By default, mutexes are unfair and allow the current thread to re-lock
    /// the mutex before another has the chance to acquire the lock, even if
    /// that thread has been blocked on the mutex for a long time. This is the
    /// default because it allows much higher throughput as it avoids forcing a
    /// context switch on every mutex unlock. This can result in one thread
    /// acquiring a mutex many more times than other threads.
    ///
    /// However in some cases it can be beneficial to ensure fairness by forcing
    /// the lock to pass on to a waiting thread if there is one. This is done by
    /// using this method instead of dropping the `ReentrantMutexGuard` normally.
    #[inline]
    pub fn unlock_fair(s: Self) {
        s.raw.unlock_fair();
        mem::forget(s);
    }
}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> Deref
    for MappedReentrantMutexGuard<'a, R, G, T>
{
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        unsafe { &*self.data }
    }
}

impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> Drop
    for MappedReentrantMutexGuard<'a, R, G, T>
{
    #[inline]
    fn drop(&mut self) {
        self.raw.unlock();
    }
}

#[cfg(feature = "owning_ref")]
unsafe impl<'a, R: RawMutex + 'a, G: GetThreadId + 'a, T: ?Sized + 'a> StableAddress
    for MappedReentrantMutexGuard<'a, R, G, T>
{}
