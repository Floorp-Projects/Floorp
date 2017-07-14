// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use std::cell::UnsafeCell;
use std::ops::Deref;
use std::time::{Duration, Instant};
use std::fmt;
use std::mem;
use std::marker::PhantomData;
use raw_remutex::RawReentrantMutex;

#[cfg(feature = "owning_ref")]
use owning_ref::StableAddress;

/// A mutex which can be recursively locked by a single thread.
///
/// This type is identical to `Mutex` except for the following points:
///
/// - Locking multiple times from the same thread will work correctly instead of
///   deadlocking.
/// - `ReentrantMutexGuard` does not give mutable references to the locked data.
///   Use a `RefCell` if you need this.
/// - `ReentrantMutexGuard` is not `Send`.
///
/// See [`Mutex`](struct.Mutex.html) for more details about the underlying mutex
/// primitive.
pub struct ReentrantMutex<T: ?Sized> {
    raw: RawReentrantMutex,
    data: UnsafeCell<T>,
}

unsafe impl<T: Send> Send for ReentrantMutex<T> {}
unsafe impl<T: Send> Sync for ReentrantMutex<T> {}

/// An RAII implementation of a "scoped lock" of a reentrant mutex. When this structure
/// is dropped (falls out of scope), the lock will be unlocked.
///
/// The data protected by the mutex can be accessed through this guard via its
/// `Deref` implementation.
#[must_use]
pub struct ReentrantMutexGuard<'a, T: ?Sized + 'a> {
    mutex: &'a ReentrantMutex<T>,

    // The raw pointer here ensures that ReentrantMutexGuard is !Send
    marker: PhantomData<(&'a T, *mut ())>,
}

unsafe impl<'a, T: ?Sized + 'a + Sync> Sync for ReentrantMutexGuard<'a, T> {}

impl<T> ReentrantMutex<T> {
    /// Creates a new reentrant mutex in an unlocked state ready for use.
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new(val: T) -> ReentrantMutex<T> {
        ReentrantMutex {
            data: UnsafeCell::new(val),
            raw: RawReentrantMutex::new(),
        }
    }

    /// Creates a new reentrant mutex in an unlocked state ready for use.
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new(val: T) -> ReentrantMutex<T> {
        ReentrantMutex {
            data: UnsafeCell::new(val),
            raw: RawReentrantMutex::new(),
        }
    }

    /// Consumes this reentrant mutex, returning the underlying data.
    #[inline]
    pub fn into_inner(self) -> T {
        unsafe { self.data.into_inner() }
    }
}

impl<T: ?Sized> ReentrantMutex<T> {
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
    pub fn lock(&self) -> ReentrantMutexGuard<T> {
        self.raw.lock();
        ReentrantMutexGuard {
            mutex: self,
            marker: PhantomData,
        }
    }

    /// Attempts to acquire this lock.
    ///
    /// If the lock could not be acquired at this time, then `None` is returned.
    /// Otherwise, an RAII guard is returned. The lock will be unlocked when the
    /// guard is dropped.
    ///
    /// This function does not block.
    #[inline]
    pub fn try_lock(&self) -> Option<ReentrantMutexGuard<T>> {
        if self.raw.try_lock() {
            Some(ReentrantMutexGuard {
                mutex: self,
                marker: PhantomData,
            })
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
    pub fn try_lock_for(&self, timeout: Duration) -> Option<ReentrantMutexGuard<T>> {
        if self.raw.try_lock_for(timeout) {
            Some(ReentrantMutexGuard {
                mutex: self,
                marker: PhantomData,
            })
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
    pub fn try_lock_until(&self, timeout: Instant) -> Option<ReentrantMutexGuard<T>> {
        if self.raw.try_lock_until(timeout) {
            Some(ReentrantMutexGuard {
                mutex: self,
                marker: PhantomData,
            })
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

    /// Releases the mutex.
    ///
    /// # Safety
    ///
    /// This function must only be called if the mutex was locked using
    /// `raw_lock` or `raw_try_lock`, or if a `ReentrantMutexGuard` from this mutex was
    /// leaked (e.g. with `mem::forget`). The mutex must be locked.
    #[inline]
    pub unsafe fn raw_unlock(&self) {
        self.raw.unlock(false);
    }

    /// Releases the mutex using a fair unlock protocol.
    ///
    /// See `ReentrantMutexGuard::unlock_fair`.
    ///
    /// # Safety
    ///
    /// This function must only be called if the mutex was locked using
    /// `raw_lock` or `raw_try_lock`, or if a `ReentrantMutexGuard` from this mutex was
    /// leaked (e.g. with `mem::forget`). The mutex must be locked.
    #[inline]
    pub unsafe fn raw_unlock_fair(&self) {
        self.raw.unlock(true);
    }
}
impl ReentrantMutex<()> {
    /// Acquires a mutex, blocking the current thread until it is able to do so.
    ///
    /// This is similar to `lock`, except that a `ReentrantMutexGuard` is not returned.
    /// Instead you will need to call `raw_unlock` to release the mutex.
    #[inline]
    pub fn raw_lock(&self) {
        self.raw.lock();
    }

    /// Attempts to acquire this lock.
    ///
    /// This is similar to `try_lock`, except that a `ReentrantMutexGuard` is not
    /// returned. Instead you will need to call `raw_unlock` to release the
    /// mutex.
    #[inline]
    pub fn raw_try_lock(&self) -> bool {
        self.raw.try_lock()
    }
}

impl<T: ?Sized + Default> Default for ReentrantMutex<T> {
    #[inline]
    fn default() -> ReentrantMutex<T> {
        ReentrantMutex::new(Default::default())
    }
}

impl<T: ?Sized + fmt::Debug> fmt::Debug for ReentrantMutex<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.try_lock() {
            Some(guard) => write!(f, "ReentrantMutex {{ data: {:?} }}", &*guard),
            None => write!(f, "ReentrantMutex {{ <locked> }}"),
        }
    }
}

impl<'a, T: ?Sized + 'a> ReentrantMutexGuard<'a, T> {
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
    pub fn unlock_fair(self) {
        self.mutex.raw.unlock(true);
        mem::forget(self);
    }
}

impl<'a, T: ?Sized + 'a> Deref for ReentrantMutexGuard<'a, T> {
    type Target = T;
    #[inline]
    fn deref(&self) -> &T {
        unsafe { &*self.mutex.data.get() }
    }
}

impl<'a, T: ?Sized + 'a> Drop for ReentrantMutexGuard<'a, T> {
    #[inline]
    fn drop(&mut self) {
        self.mutex.raw.unlock(false);
    }
}

#[cfg(feature = "owning_ref")]
unsafe impl<'a, T: ?Sized> StableAddress for ReentrantMutexGuard<'a, T> {}

#[cfg(test)]
mod tests {
    use std::cell::RefCell;
    use std::sync::Arc;
    use std::thread;
    use ReentrantMutex;

    #[test]
    fn smoke() {
        let m = ReentrantMutex::new(());
        {
            let a = m.lock();
            {
                let b = m.lock();
                {
                    let c = m.lock();
                    assert_eq!(*c, ());
                }
                assert_eq!(*b, ());
            }
            assert_eq!(*a, ());
        }
    }

    #[test]
    fn is_mutex() {
        let m = Arc::new(ReentrantMutex::new(RefCell::new(0)));
        let m2 = m.clone();
        let lock = m.lock();
        let child = thread::spawn(move || {
            let lock = m2.lock();
            assert_eq!(*lock.borrow(), 4950);
        });
        for i in 0..100 {
            let lock = m.lock();
            *lock.borrow_mut() += i;
        }
        drop(lock);
        child.join().unwrap();
    }

    #[test]
    fn trylock_works() {
        let m = Arc::new(ReentrantMutex::new(()));
        let m2 = m.clone();
        let _lock = m.try_lock();
        let _lock2 = m.try_lock();
        thread::spawn(move || {
                let lock = m2.try_lock();
                assert!(lock.is_none());
            })
            .join()
            .unwrap();
        let _lock3 = m.try_lock();
    }
}
