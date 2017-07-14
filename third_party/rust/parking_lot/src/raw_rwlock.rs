// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(feature = "nightly")]
use std::sync::atomic::{AtomicUsize, Ordering};
use std::cell::Cell;
#[cfg(not(feature = "nightly"))]
use stable::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};
use parking_lot_core::{self, ParkResult, UnparkResult, SpinWait, ParkToken, FilterOp};
use elision::{have_elision, AtomicElisionExt};
use util::UncheckedOptionExt;
use raw_mutex::{TOKEN_NORMAL, TOKEN_HANDOFF};

// Token indicating what type of lock queued threads are trying to acquire
const TOKEN_SHARED: ParkToken = ParkToken(0);
const TOKEN_EXCLUSIVE: ParkToken = ParkToken(1);

const PARKED_BIT: usize = 1;
const LOCKED_BIT: usize = 2;
const SHARED_COUNT_MASK: usize = !3;
const SHARED_COUNT_INC: usize = 4;
const SHARED_COUNT_SHIFT: usize = 2;

pub struct RawRwLock {
    state: AtomicUsize,
}

impl RawRwLock {
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new() -> RawRwLock {
        RawRwLock { state: AtomicUsize::new(0) }
    }
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new() -> RawRwLock {
        RawRwLock { state: AtomicUsize::new(0) }
    }

    #[inline]
    pub fn lock_exclusive(&self) {
        if self.state
            .compare_exchange_weak(0, LOCKED_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok() {
            return;
        }
        self.lock_exclusive_slow(None);
    }

    #[inline]
    pub fn try_lock_exclusive_until(&self, timeout: Instant) -> bool {
        if self.state
            .compare_exchange_weak(0, LOCKED_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok() {
            return true;
        }
        self.lock_exclusive_slow(Some(timeout))
    }

    #[inline]
    pub fn try_lock_exclusive_for(&self, timeout: Duration) -> bool {
        if self.state
            .compare_exchange_weak(0, LOCKED_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok() {
            return true;
        }
        self.lock_exclusive_slow(Some(Instant::now() + timeout))
    }

    #[inline]
    pub fn try_lock_exclusive(&self) -> bool {
        self.state
            .compare_exchange(0, LOCKED_BIT, Ordering::Acquire, Ordering::Relaxed)
            .is_ok()
    }

    #[inline]
    pub fn unlock_exclusive(&self, force_fair: bool) {
        if self.state
            .compare_exchange_weak(LOCKED_BIT, 0, Ordering::Release, Ordering::Relaxed)
            .is_ok() {
            return;
        }
        self.unlock_exclusive_slow(force_fair);
    }

    #[inline]
    pub fn downgrade(&self) {
        let state = self.state
            .fetch_add(SHARED_COUNT_INC - LOCKED_BIT, Ordering::Release);

        // Wake up parked shared threads if there are any
        if state & PARKED_BIT != 0 {
            self.downgrade_slow();
        }
    }

    #[inline(always)]
    fn try_lock_shared_fast(&self, recursive: bool) -> bool {
        let state = self.state.load(Ordering::Relaxed);

        if !recursive {
            // Even if there are no exclusive locks, we can't allow grabbing a
            // shared lock while there are parked threads since that could lead to
            // writer starvation.
            if state & (LOCKED_BIT | PARKED_BIT) != 0 {
                return false;
            }
        } else {
            // Allow acquiring a lock even if a thread is parked to avoid
            // deadlocks for recursive read locks.
            if state & LOCKED_BIT != 0 {
                return false;
            }
        }

        // Use hardware lock elision to avoid cache conflicts when multiple
        // readers try to acquire the lock. We only do this if the lock is
        // completely empty since elision handles conflicts poorly.
        if have_elision() && state == 0 {
            self.state.elision_acquire(0, SHARED_COUNT_INC).is_ok()
        } else if let Some(new_state) = state.checked_add(SHARED_COUNT_INC) {
            self.state
                .compare_exchange_weak(state, new_state, Ordering::Acquire, Ordering::Relaxed)
                .is_ok()
        } else {
            false
        }
    }

    #[inline]
    pub fn lock_shared(&self, recursive: bool) {
        if !self.try_lock_shared_fast(recursive) {
            self.lock_shared_slow(recursive, None);
        }
    }

    #[inline]
    pub fn try_lock_shared_until(&self, recursive: bool, timeout: Instant) -> bool {
        if self.try_lock_shared_fast(recursive) {
            return true;
        }
        self.lock_shared_slow(recursive, Some(timeout))
    }

    #[inline]
    pub fn try_lock_shared_for(&self, recursive: bool, timeout: Duration) -> bool {
        if self.try_lock_shared_fast(recursive) {
            return true;
        }
        self.lock_shared_slow(recursive, Some(Instant::now() + timeout))
    }

    #[inline]
    pub fn try_lock_shared(&self, recursive: bool) -> bool {
        if self.try_lock_shared_fast(recursive) {
            return true;
        }
        self.try_lock_shared_slow(recursive)
    }

    #[inline]
    pub fn unlock_shared(&self, force_fair: bool) {
        let state = self.state.load(Ordering::Relaxed);
        if state & PARKED_BIT == 0 || state & SHARED_COUNT_MASK != SHARED_COUNT_INC {
            if have_elision() {
                if self.state.elision_release(state, state - SHARED_COUNT_INC).is_ok() {
                    return;
                }
            } else {
                if self.state
                    .compare_exchange_weak(state,
                                           state - SHARED_COUNT_INC,
                                           Ordering::Release,
                                           Ordering::Relaxed)
                    .is_ok() {
                    return;
                }
            }
        }
        self.unlock_shared_slow(force_fair);
    }

    #[cold]
    #[inline(never)]
    fn lock_exclusive_slow(&self, timeout: Option<Instant>) -> bool {
        let mut spinwait = SpinWait::new();
        let mut state = self.state.load(Ordering::Relaxed);
        loop {
            // Grab the lock if it isn't locked, even if there are other
            // threads parked.
            if state & (LOCKED_BIT | SHARED_COUNT_MASK) == 0 {
                match self.state
                    .compare_exchange_weak(state,
                                           state | LOCKED_BIT,
                                           Ordering::Acquire,
                                           Ordering::Relaxed) {
                    Ok(_) => return true,
                    Err(x) => state = x,
                }
                continue;
            }

            // If there are no parked threads and only one reader or writer, try
            // spinning a few times.
            if state & PARKED_BIT == 0 &&
               (state & LOCKED_BIT != 0 || state & SHARED_COUNT_MASK == SHARED_COUNT_INC) &&
               spinwait.spin() {
                state = self.state.load(Ordering::Relaxed);
                continue;
            }

            // Park our thread until we are woken up by an unlock
            unsafe {
                let addr = self as *const _ as usize;
                let validate = || {
                    let mut state = self.state.load(Ordering::Relaxed);
                    loop {
                        // If the rwlock is free, abort the park and try to grab
                        // it immediately.
                        if state & (LOCKED_BIT | SHARED_COUNT_MASK) == 0 {
                            return false;
                        }

                        // Nothing to do if the parked bit is already set
                        if state & PARKED_BIT != 0 {
                            return true;
                        }

                        // Set the parked bit
                        match self.state.compare_exchange_weak(state,
                                                               state | PARKED_BIT,
                                                               Ordering::Relaxed,
                                                               Ordering::Relaxed) {
                            Ok(_) => return true,
                            Err(x) => state = x,
                        }
                    }
                };
                let before_sleep = || {};
                let timed_out = |_, was_last_thread| {
                    // Clear the parked bit if we were the last parked thread
                    if was_last_thread {
                        self.state.fetch_and(!PARKED_BIT, Ordering::Relaxed);
                    }
                };
                match parking_lot_core::park(addr,
                                             validate,
                                             before_sleep,
                                             timed_out,
                                             TOKEN_EXCLUSIVE,
                                             timeout) {
                    // The thread that unparked us passed the lock on to us
                    // directly without unlocking it.
                    ParkResult::Unparked(TOKEN_HANDOFF) => return true,

                    // We were unparked normally, try acquiring the lock again
                    ParkResult::Unparked(_) => (),

                    // The validation function failed, try locking again
                    ParkResult::Invalid => (),

                    // Timeout expired
                    ParkResult::TimedOut => return false,
                }
            }

            // Loop back and try locking again
            spinwait.reset();
            state = self.state.load(Ordering::Relaxed);
        }
    }

    #[cold]
    #[inline(never)]
    fn unlock_exclusive_slow(&self, force_fair: bool) {
        // Unlock directly if there are no parked threads
        if self.state
            .compare_exchange(LOCKED_BIT, 0, Ordering::Release, Ordering::Relaxed)
            .is_ok() {
            return;
        }

        // There are threads to unpark. We can unpark a single exclusive
        // thread or many shared threads.
        let first_token = Cell::new(None);
        unsafe {
            let addr = self as *const _ as usize;
            let filter = |token| -> FilterOp {
                if let Some(first_token) = first_token.get() {
                    if first_token == TOKEN_EXCLUSIVE || token == TOKEN_EXCLUSIVE {
                        FilterOp::Stop
                    } else {
                        FilterOp::Unpark
                    }
                } else {
                    first_token.set(Some(token));
                    FilterOp::Unpark
                }
            };
            let callback = |result: UnparkResult| {
                // If we are using a fair unlock then we should keep the
                // rwlock locked and hand it off to the unparked threads.
                if result.unparked_threads != 0 && (force_fair || result.be_fair) {
                    if first_token.get().unchecked_unwrap() == TOKEN_EXCLUSIVE {
                        // If we unparked an exclusive thread, just clear the
                        // parked bit if there are no more parked threads.
                        if !result.have_more_threads {
                            self.state.store(LOCKED_BIT, Ordering::Relaxed);
                        }
                    } else {
                        // If we unparked shared threads then we need to set
                        // the shared count accordingly.
                        if result.have_more_threads {
                            self.state.store((result.unparked_threads << SHARED_COUNT_SHIFT) |
                                             PARKED_BIT,
                                             Ordering::Release);
                        } else {
                            self.state.store(result.unparked_threads << SHARED_COUNT_SHIFT,
                                             Ordering::Release);
                        }
                    }
                    return TOKEN_HANDOFF;
                }

                // Clear the locked bit, and the parked bit as well if there
                // are no more parked threads.
                if result.have_more_threads {
                    self.state.store(PARKED_BIT, Ordering::Release);
                } else {
                    self.state.store(0, Ordering::Release);
                }
                TOKEN_NORMAL
            };
            parking_lot_core::unpark_filter(addr, filter, callback);
        }
    }

    #[cold]
    #[inline(never)]
    fn downgrade_slow(&self) {
        // Unpark shared threads only
        unsafe {
            let addr = self as *const _ as usize;
            let filter = |token| -> FilterOp {
                if token == TOKEN_SHARED {
                    FilterOp::Unpark
                } else {
                    FilterOp::Stop
                }
            };
            let callback = |result: UnparkResult| {
                // Clear the parked bit if there no more parked threads
                if !result.have_more_threads {
                    self.state.fetch_and(!PARKED_BIT, Ordering::Relaxed);
                }
                TOKEN_NORMAL
            };
            parking_lot_core::unpark_filter(addr, filter, callback);
        }
    }

    #[cold]
    #[inline(never)]
    fn lock_shared_slow(&self, recursive: bool, timeout: Option<Instant>) -> bool {
        let mut spinwait = SpinWait::new();
        let mut spinwait_shared = SpinWait::new();
        let mut state = self.state.load(Ordering::Relaxed);
        let mut unparked = false;
        loop {
            // Use hardware lock elision to avoid cache conflicts when multiple
            // readers try to acquire the lock. We only do this if the lock is
            // completely empty since elision handles conflicts poorly.
            if have_elision() && state == 0 {
                match self.state.elision_acquire(0, SHARED_COUNT_INC) {
                    Ok(_) => return true,
                    Err(x) => state = x,
                }
            }

            // Grab the lock if there are no exclusive threads locked or
            // waiting. However if we were unparked then we are allowed to grab
            // the lock even if there are pending exclusive threads.
            if state & LOCKED_BIT == 0 && (unparked || recursive || state & PARKED_BIT == 0) {
                let new = state.checked_add(SHARED_COUNT_INC)
                    .expect("RwLock shared count overflow");
                if self.state
                    .compare_exchange_weak(state, new, Ordering::Acquire, Ordering::Relaxed)
                    .is_ok() {
                    return true;
                }

                // If there is high contention on the reader count then we want
                // to leave some time between attempts to acquire the lock to
                // let other threads make progress.
                spinwait_shared.spin_no_yield();
                state = self.state.load(Ordering::Relaxed);
                continue;
            }

            // If there are no parked threads, try spinning a few times
            if state & PARKED_BIT == 0 && spinwait.spin() {
                state = self.state.load(Ordering::Relaxed);
                continue;
            }

            // Park our thread until we are woken up by an unlock
            unsafe {
                let addr = self as *const _ as usize;
                let validate = || {
                    let mut state = self.state.load(Ordering::Relaxed);
                    loop {
                        // Nothing to do if the parked bit is already set
                        if state & PARKED_BIT != 0 {
                            return true;
                        }

                        // If the parked bit is not set then it means we are at
                        // the front of the queue. If there is no exclusive lock
                        // then we should abort the park and try acquiring the
                        // lock again.
                        if state & LOCKED_BIT == 0 {
                            return false;
                        }

                        // Set the parked bit
                        match self.state.compare_exchange_weak(state,
                                                               state | PARKED_BIT,
                                                               Ordering::Relaxed,
                                                               Ordering::Relaxed) {
                            Ok(_) => return true,
                            Err(x) => state = x,
                        }
                    }
                };
                let before_sleep = || {};
                let timed_out = |_, was_last_thread| {
                    // Clear the parked bit if we were the last parked thread
                    if was_last_thread {
                        self.state.fetch_and(!PARKED_BIT, Ordering::Relaxed);
                    }
                };
                match parking_lot_core::park(addr,
                                             validate,
                                             before_sleep,
                                             timed_out,
                                             TOKEN_SHARED,
                                             timeout) {
                    // The thread that unparked us passed the lock on to us
                    // directly without unlocking it.
                    ParkResult::Unparked(TOKEN_HANDOFF) => return true,

                    // We were unparked normally, try acquiring the lock again
                    ParkResult::Unparked(_) => (),

                    // The validation function failed, try locking again
                    ParkResult::Invalid => (),

                    // Timeout expired
                    ParkResult::TimedOut => return false,
                }
            }

            // Loop back and try locking again
            spinwait.reset();
            spinwait_shared.reset();
            state = self.state.load(Ordering::Relaxed);
            unparked = true;
        }
    }

    #[cold]
    #[inline(never)]
    pub fn try_lock_shared_slow(&self, recursive: bool) -> bool {
        let mut state = self.state.load(Ordering::Relaxed);
        loop {
            let mask = if recursive {
                LOCKED_BIT
            } else {
                LOCKED_BIT | PARKED_BIT
            };
            if state & mask != 0 {
                return false;
            }
            if have_elision() && state == 0 {
                match self.state.elision_acquire(0, SHARED_COUNT_INC) {
                    Ok(_) => return true,
                    Err(x) => state = x,
                }
            } else {
                let new = state.checked_add(SHARED_COUNT_INC)
                    .expect("RwLock shared count overflow");
                match self.state
                    .compare_exchange_weak(state, new, Ordering::Acquire, Ordering::Relaxed) {
                    Ok(_) => return true,
                    Err(x) => state = x,
                }
            }
        }
    }

    #[cold]
    #[inline(never)]
    fn unlock_shared_slow(&self, force_fair: bool) {
        let mut state = self.state.load(Ordering::Relaxed);
        loop {
            // Just release the lock if there are no parked thread or if we are
            // not the last shared thread.
            if state & PARKED_BIT == 0 || state & SHARED_COUNT_MASK != SHARED_COUNT_INC {
                match self.state
                    .compare_exchange_weak(state,
                                           state - SHARED_COUNT_INC,
                                           Ordering::Release,
                                           Ordering::Relaxed) {
                    Ok(_) => return,
                    Err(x) => state = x,
                }
                continue;
            }

            // There are threads to unpark. We can unpark a single exclusive
            // thread or many shared threads. Note that there is a potential
            // race condition here: another thread might grab a shared lock
            // between now and when we actually release our lock.
            let first_token = Cell::new(None);
            unsafe {
                let addr = self as *const _ as usize;
                let filter = |token| -> FilterOp {
                    if let Some(first_token) = first_token.get() {
                        if first_token == TOKEN_EXCLUSIVE || token == TOKEN_EXCLUSIVE {
                            FilterOp::Stop
                        } else {
                            FilterOp::Unpark
                        }
                    } else {
                        first_token.set(Some(token));
                        FilterOp::Unpark
                    }
                };
                let callback = |result: UnparkResult| {
                    let mut state = self.state.load(Ordering::Relaxed);
                    loop {
                        // Release our shared lock
                        let mut new = state - SHARED_COUNT_INC;

                        // Clear the parked bit if there are no more threads in
                        // the queue
                        if !result.have_more_threads {
                            new &= !PARKED_BIT;
                        }

                        // If we are the last shared thread and we unparked an
                        // exclusive thread then we can consider using fair
                        // unlocking. If we are then we should set the exclusive
                        // locked bit and tell the thread that we are handing it
                        // the lock directly.
                        let token = if result.unparked_threads != 0 &&
                                       new & SHARED_COUNT_MASK == 0 &&
                                       first_token.get().unchecked_unwrap() == TOKEN_EXCLUSIVE &&
                                       (force_fair || result.be_fair) {
                            new |= LOCKED_BIT;
                            TOKEN_HANDOFF
                        } else {
                            TOKEN_NORMAL
                        };

                        match self.state
                            .compare_exchange_weak(state,
                                                   new,
                                                   Ordering::Release,
                                                   Ordering::Relaxed) {
                            Ok(_) => return token,
                            Err(x) => state = x,
                        }
                    }
                };
                parking_lot_core::unpark_filter(addr, filter, callback);
            }
            break;
        }
    }
}
