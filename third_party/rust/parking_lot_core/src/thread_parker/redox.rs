// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use core::{
    ptr,
    sync::atomic::{AtomicI32, Ordering},
};
use std::{thread, time::Instant};
use syscall::{
    call::futex,
    data::TimeSpec,
    error::{Error, EAGAIN, EFAULT, EINTR, ETIMEDOUT},
    flag::{FUTEX_WAIT, FUTEX_WAKE},
};

const UNPARKED: i32 = 0;
const PARKED: i32 = 1;

// Helper type for putting a thread to sleep until some other thread wakes it up
pub struct ThreadParker {
    futex: AtomicI32,
}

impl ThreadParker {
    pub const IS_CHEAP_TO_CONSTRUCT: bool = true;

    #[inline]
    pub fn new() -> ThreadParker {
        ThreadParker {
            futex: AtomicI32::new(UNPARKED),
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    #[inline]
    pub fn prepare_park(&self) {
        self.futex.store(PARKED, Ordering::Relaxed);
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    #[inline]
    pub fn timed_out(&self) -> bool {
        self.futex.load(Ordering::Relaxed) != UNPARKED
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    #[inline]
    pub fn park(&self) {
        while self.futex.load(Ordering::Acquire) != UNPARKED {
            self.futex_wait(None);
        }
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    #[inline]
    pub fn park_until(&self, timeout: Instant) -> bool {
        while self.futex.load(Ordering::Acquire) != UNPARKED {
            let now = Instant::now();
            if timeout <= now {
                return false;
            }
            let diff = timeout - now;
            if diff.as_secs() > i64::max_value() as u64 {
                // Timeout overflowed, just sleep indefinitely
                self.park();
                return true;
            }
            let ts = TimeSpec {
                tv_sec: diff.as_secs() as i64,
                tv_nsec: diff.subsec_nanos() as i32,
            };
            self.futex_wait(Some(ts));
        }
        true
    }

    #[inline]
    fn futex_wait(&self, ts: Option<TimeSpec>) {
        let ts_ptr = ts
            .as_ref()
            .map(|ts_ref| ts_ref as *const _)
            .unwrap_or(ptr::null());
        let r = unsafe {
            futex(
                self.ptr(),
                FUTEX_WAIT,
                PARKED,
                ts_ptr as usize,
                ptr::null_mut(),
            )
        };
        match r {
            Ok(r) => debug_assert_eq!(r, 0),
            Err(Error { errno }) => {
                debug_assert!(errno == EINTR || errno == EAGAIN || errno == ETIMEDOUT);
            }
        }
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    #[inline]
    pub fn unpark_lock(&self) -> UnparkHandle {
        // We don't need to lock anything, just clear the state
        self.futex.store(UNPARKED, Ordering::Release);

        UnparkHandle { futex: self.ptr() }
    }

    #[inline]
    fn ptr(&self) -> *mut i32 {
        &self.futex as *const AtomicI32 as *mut i32
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle {
    futex: *mut i32,
}

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    #[inline]
    pub fn unpark(self) {
        // The thread data may have been freed at this point, but it doesn't
        // matter since the syscall will just return EFAULT in that case.
        let r = unsafe { futex(self.futex, FUTEX_WAKE, PARKED, 0, ptr::null_mut()) };
        match r {
            Ok(num_woken) => debug_assert!(num_woken == 0 || num_woken == 1),
            Err(Error { errno }) => debug_assert_eq!(errno, EFAULT),
        }
    }
}

#[inline]
pub fn thread_yield() {
    thread::yield_now();
}
