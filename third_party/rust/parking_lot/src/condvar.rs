// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use deadlock;
use lock_api::RawMutex as RawMutexTrait;
use mutex::MutexGuard;
use parking_lot_core::{self, ParkResult, RequeueOp, UnparkResult, DEFAULT_PARK_TOKEN};
use raw_mutex::{RawMutex, TOKEN_HANDOFF, TOKEN_NORMAL};
use std::sync::atomic::{AtomicPtr, Ordering};
use std::time::{Duration, Instant};
use std::{fmt, ptr};

/// A type indicating whether a timed wait on a condition variable returned
/// due to a time out or not.
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct WaitTimeoutResult(bool);

impl WaitTimeoutResult {
    /// Returns whether the wait was known to have timed out.
    #[inline]
    pub fn timed_out(&self) -> bool {
        self.0
    }
}

/// A Condition Variable
///
/// Condition variables represent the ability to block a thread such that it
/// consumes no CPU time while waiting for an event to occur. Condition
/// variables are typically associated with a boolean predicate (a condition)
/// and a mutex. The predicate is always verified inside of the mutex before
/// determining that thread must block.
///
/// Note that this module places one additional restriction over the system
/// condition variables: each condvar can be used with only one mutex at a
/// time. Any attempt to use multiple mutexes on the same condition variable
/// simultaneously will result in a runtime panic. However it is possible to
/// switch to a different mutex if there are no threads currently waiting on
/// the condition variable.
///
/// # Differences from the standard library `Condvar`
///
/// - No spurious wakeups: A wait will only return a non-timeout result if it
///   was woken up by `notify_one` or `notify_all`.
/// - `Condvar::notify_all` will only wake up a single thread, the rest are
///   requeued to wait for the `Mutex` to be unlocked by the thread that was
///   woken up.
/// - Only requires 1 word of space, whereas the standard library boxes the
///   `Condvar` due to platform limitations.
/// - Can be statically constructed (requires the `const_fn` nightly feature).
/// - Does not require any drop glue when dropped.
/// - Inline fast path for the uncontended case.
///
/// # Examples
///
/// ```
/// use parking_lot::{Mutex, Condvar};
/// use std::sync::Arc;
/// use std::thread;
///
/// let pair = Arc::new((Mutex::new(false), Condvar::new()));
/// let pair2 = pair.clone();
///
/// // Inside of our lock, spawn a new thread, and then wait for it to start
/// thread::spawn(move|| {
///     let &(ref lock, ref cvar) = &*pair2;
///     let mut started = lock.lock();
///     *started = true;
///     cvar.notify_one();
/// });
///
/// // wait for the thread to start up
/// let &(ref lock, ref cvar) = &*pair;
/// let mut started = lock.lock();
/// while !*started {
///     cvar.wait(&mut started);
/// }
/// ```
pub struct Condvar {
    state: AtomicPtr<RawMutex>,
}

impl Condvar {
    /// Creates a new condition variable which is ready to be waited on and
    /// notified.
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new() -> Condvar {
        Condvar {
            state: AtomicPtr::new(ptr::null_mut()),
        }
    }

    /// Creates a new condition variable which is ready to be waited on and
    /// notified.
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new() -> Condvar {
        Condvar {
            state: AtomicPtr::new(ptr::null_mut()),
        }
    }

    /// Wakes up one blocked thread on this condvar.
    ///
    /// If there is a blocked thread on this condition variable, then it will
    /// be woken up from its call to `wait` or `wait_timeout`. Calls to
    /// `notify_one` are not buffered in any way.
    ///
    /// To wake up all threads, see `notify_all()`.
    #[inline]
    pub fn notify_one(&self) {
        // Nothing to do if there are no waiting threads
        if self.state.load(Ordering::Relaxed).is_null() {
            return;
        }

        self.notify_one_slow();
    }

    #[cold]
    #[inline(never)]
    fn notify_one_slow(&self) {
        unsafe {
            // Unpark one thread
            let addr = self as *const _ as usize;
            let callback = |result: UnparkResult| {
                // Clear our state if there are no more waiting threads
                if !result.have_more_threads {
                    self.state.store(ptr::null_mut(), Ordering::Relaxed);
                }
                TOKEN_NORMAL
            };
            parking_lot_core::unpark_one(addr, callback);
        }
    }

    /// Wakes up all blocked threads on this condvar.
    ///
    /// This method will ensure that any current waiters on the condition
    /// variable are awoken. Calls to `notify_all()` are not buffered in any
    /// way.
    ///
    /// To wake up only one thread, see `notify_one()`.
    #[inline]
    pub fn notify_all(&self) {
        // Nothing to do if there are no waiting threads
        let state = self.state.load(Ordering::Relaxed);
        if state.is_null() {
            return;
        }

        self.notify_all_slow(state);
    }

    #[cold]
    #[inline(never)]
    fn notify_all_slow(&self, mutex: *mut RawMutex) {
        unsafe {
            // Unpark one thread and requeue the rest onto the mutex
            let from = self as *const _ as usize;
            let to = mutex as usize;
            let validate = || {
                // Make sure that our atomic state still points to the same
                // mutex. If not then it means that all threads on the current
                // mutex were woken up and a new waiting thread switched to a
                // different mutex. In that case we can get away with doing
                // nothing.
                if self.state.load(Ordering::Relaxed) != mutex {
                    return RequeueOp::Abort;
                }

                // Clear our state since we are going to unpark or requeue all
                // threads.
                self.state.store(ptr::null_mut(), Ordering::Relaxed);

                // Unpark one thread if the mutex is unlocked, otherwise just
                // requeue everything to the mutex. This is safe to do here
                // since unlocking the mutex when the parked bit is set requires
                // locking the queue. There is the possibility of a race if the
                // mutex gets locked after we check, but that doesn't matter in
                // this case.
                if (*mutex).mark_parked_if_locked() {
                    RequeueOp::RequeueAll
                } else {
                    RequeueOp::UnparkOneRequeueRest
                }
            };
            let callback = |op, result: UnparkResult| {
                // If we requeued threads to the mutex, mark it as having
                // parked threads. The RequeueAll case is already handled above.
                if op == RequeueOp::UnparkOneRequeueRest && result.have_more_threads {
                    (*mutex).mark_parked();
                }
                TOKEN_NORMAL
            };
            parking_lot_core::unpark_requeue(from, to, validate, callback);
        }
    }

    /// Blocks the current thread until this condition variable receives a
    /// notification.
    ///
    /// This function will atomically unlock the mutex specified (represented by
    /// `mutex_guard`) and block the current thread. This means that any calls
    /// to `notify_*()` which happen logically after the mutex is unlocked are
    /// candidates to wake this thread up. When this function call returns, the
    /// lock specified will have been re-acquired.
    ///
    /// # Panics
    ///
    /// This function will panic if another thread is waiting on the `Condvar`
    /// with a different `Mutex` object.
    #[inline]
    pub fn wait<T: ?Sized>(&self, mutex_guard: &mut MutexGuard<T>) {
        self.wait_until_internal(unsafe { MutexGuard::mutex(mutex_guard).raw() }, None);
    }

    /// Waits on this condition variable for a notification, timing out after
    /// the specified time instant.
    ///
    /// The semantics of this function are equivalent to `wait()` except that
    /// the thread will be blocked roughly until `timeout` is reached. This
    /// method should not be used for precise timing due to anomalies such as
    /// preemption or platform differences that may not cause the maximum
    /// amount of time waited to be precisely `timeout`.
    ///
    /// Note that the best effort is made to ensure that the time waited is
    /// measured with a monotonic clock, and not affected by the changes made to
    /// the system time.
    ///
    /// The returned `WaitTimeoutResult` value indicates if the timeout is
    /// known to have elapsed.
    ///
    /// Like `wait`, the lock specified will be re-acquired when this function
    /// returns, regardless of whether the timeout elapsed or not.
    ///
    /// # Panics
    ///
    /// This function will panic if another thread is waiting on the `Condvar`
    /// with a different `Mutex` object.
    #[inline]
    pub fn wait_until<T: ?Sized>(
        &self,
        mutex_guard: &mut MutexGuard<T>,
        timeout: Instant,
    ) -> WaitTimeoutResult {
        self.wait_until_internal(
            unsafe { MutexGuard::mutex(mutex_guard).raw() },
            Some(timeout),
        )
    }

    // This is a non-generic function to reduce the monomorphization cost of
    // using `wait_until`.
    fn wait_until_internal(
        &self,
        mutex: &RawMutex,
        timeout: Option<Instant>,
    ) -> WaitTimeoutResult {
        unsafe {
            let result;
            let mut bad_mutex = false;
            let mut requeued = false;
            {
                let addr = self as *const _ as usize;
                let lock_addr = mutex as *const _ as *mut _;
                let validate = || {
                    // Ensure we don't use two different mutexes with the same
                    // Condvar at the same time. This is done while locked to
                    // avoid races with notify_one
                    let state = self.state.load(Ordering::Relaxed);
                    if state.is_null() {
                        self.state.store(lock_addr, Ordering::Relaxed);
                    } else if state != lock_addr {
                        bad_mutex = true;
                        return false;
                    }
                    true
                };
                let before_sleep = || {
                    // Unlock the mutex before sleeping...
                    mutex.unlock();
                };
                let timed_out = |k, was_last_thread| {
                    // If we were requeued to a mutex, then we did not time out.
                    // We'll just park ourselves on the mutex again when we try
                    // to lock it later.
                    requeued = k != addr;

                    // If we were the last thread on the queue then we need to
                    // clear our state. This is normally done by the
                    // notify_{one,all} functions when not timing out.
                    if !requeued && was_last_thread {
                        self.state.store(ptr::null_mut(), Ordering::Relaxed);
                    }
                };
                result = parking_lot_core::park(
                    addr,
                    validate,
                    before_sleep,
                    timed_out,
                    DEFAULT_PARK_TOKEN,
                    timeout,
                );
            }

            // Panic if we tried to use multiple mutexes with a Condvar. Note
            // that at this point the MutexGuard is still locked. It will be
            // unlocked by the unwinding logic.
            if bad_mutex {
                panic!("attempted to use a condition variable with more than one mutex");
            }

            // ... and re-lock it once we are done sleeping
            if result == ParkResult::Unparked(TOKEN_HANDOFF) {
                deadlock::acquire_resource(mutex as *const _ as usize);
            } else {
                mutex.lock();
            }

            WaitTimeoutResult(!(result.is_unparked() || requeued))
        }
    }

    /// Waits on this condition variable for a notification, timing out after a
    /// specified duration.
    ///
    /// The semantics of this function are equivalent to `wait()` except that
    /// the thread will be blocked for roughly no longer than `timeout`. This
    /// method should not be used for precise timing due to anomalies such as
    /// preemption or platform differences that may not cause the maximum
    /// amount of time waited to be precisely `timeout`.
    ///
    /// Note that the best effort is made to ensure that the time waited is
    /// measured with a monotonic clock, and not affected by the changes made to
    /// the system time.
    ///
    /// The returned `WaitTimeoutResult` value indicates if the timeout is
    /// known to have elapsed.
    ///
    /// Like `wait`, the lock specified will be re-acquired when this function
    /// returns, regardless of whether the timeout elapsed or not.
    #[inline]
    pub fn wait_for<T: ?Sized>(
        &self,
        guard: &mut MutexGuard<T>,
        timeout: Duration,
    ) -> WaitTimeoutResult {
        self.wait_until(guard, Instant::now() + timeout)
    }
}

impl Default for Condvar {
    #[inline]
    fn default() -> Condvar {
        Condvar::new()
    }
}

impl fmt::Debug for Condvar {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad("Condvar { .. }")
    }
}

#[cfg(test)]
mod tests {
    use std::sync::mpsc::channel;
    use std::sync::Arc;
    use std::thread;
    use std::time::{Duration, Instant};
    use {Condvar, Mutex};

    #[test]
    fn smoke() {
        let c = Condvar::new();
        c.notify_one();
        c.notify_all();
    }

    #[test]
    fn notify_one() {
        let m = Arc::new(Mutex::new(()));
        let m2 = m.clone();
        let c = Arc::new(Condvar::new());
        let c2 = c.clone();

        let mut g = m.lock();
        let _t = thread::spawn(move || {
            let _g = m2.lock();
            c2.notify_one();
        });
        c.wait(&mut g);
    }

    #[test]
    fn notify_all() {
        const N: usize = 10;

        let data = Arc::new((Mutex::new(0), Condvar::new()));
        let (tx, rx) = channel();
        for _ in 0..N {
            let data = data.clone();
            let tx = tx.clone();
            thread::spawn(move || {
                let &(ref lock, ref cond) = &*data;
                let mut cnt = lock.lock();
                *cnt += 1;
                if *cnt == N {
                    tx.send(()).unwrap();
                }
                while *cnt != 0 {
                    cond.wait(&mut cnt);
                }
                tx.send(()).unwrap();
            });
        }
        drop(tx);

        let &(ref lock, ref cond) = &*data;
        rx.recv().unwrap();
        let mut cnt = lock.lock();
        *cnt = 0;
        cond.notify_all();
        drop(cnt);

        for _ in 0..N {
            rx.recv().unwrap();
        }
    }

    #[test]
    fn wait_for() {
        let m = Arc::new(Mutex::new(()));
        let m2 = m.clone();
        let c = Arc::new(Condvar::new());
        let c2 = c.clone();

        let mut g = m.lock();
        let no_timeout = c.wait_for(&mut g, Duration::from_millis(1));
        assert!(no_timeout.timed_out());
        let _t = thread::spawn(move || {
            let _g = m2.lock();
            c2.notify_one();
        });
        let timeout_res = c.wait_for(&mut g, Duration::from_millis(u32::max_value() as u64));
        assert!(!timeout_res.timed_out());
        drop(g);
    }

    #[test]
    fn wait_until() {
        let m = Arc::new(Mutex::new(()));
        let m2 = m.clone();
        let c = Arc::new(Condvar::new());
        let c2 = c.clone();

        let mut g = m.lock();
        let no_timeout = c.wait_until(&mut g, Instant::now() + Duration::from_millis(1));
        assert!(no_timeout.timed_out());
        let _t = thread::spawn(move || {
            let _g = m2.lock();
            c2.notify_one();
        });
        let timeout_res = c.wait_until(
            &mut g,
            Instant::now() + Duration::from_millis(u32::max_value() as u64),
        );
        assert!(!timeout_res.timed_out());
        drop(g);
    }

    #[test]
    #[should_panic]
    fn two_mutexes() {
        let m = Arc::new(Mutex::new(()));
        let m2 = m.clone();
        let m3 = Arc::new(Mutex::new(()));
        let c = Arc::new(Condvar::new());
        let c2 = c.clone();

        // Make sure we don't leave the child thread dangling
        struct PanicGuard<'a>(&'a Condvar);
        impl<'a> Drop for PanicGuard<'a> {
            fn drop(&mut self) {
                self.0.notify_one();
            }
        }

        let (tx, rx) = channel();
        let g = m.lock();
        let _t = thread::spawn(move || {
            let mut g = m2.lock();
            tx.send(()).unwrap();
            c2.wait(&mut g);
        });
        drop(g);
        rx.recv().unwrap();
        let _g = m.lock();
        let _guard = PanicGuard(&*c);
        let _ = c.wait(&mut m3.lock());
    }

    #[test]
    fn two_mutexes_disjoint() {
        let m = Arc::new(Mutex::new(()));
        let m2 = m.clone();
        let m3 = Arc::new(Mutex::new(()));
        let c = Arc::new(Condvar::new());
        let c2 = c.clone();

        let mut g = m.lock();
        let _t = thread::spawn(move || {
            let _g = m2.lock();
            c2.notify_one();
        });
        c.wait(&mut g);
        drop(g);

        let _ = c.wait_for(&mut m3.lock(), Duration::from_millis(1));
    }

    #[test]
    fn test_debug_condvar() {
        let c = Condvar::new();
        assert_eq!(format!("{:?}", c), "Condvar { .. }");
    }
}
