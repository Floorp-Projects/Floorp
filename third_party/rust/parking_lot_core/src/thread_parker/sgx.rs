// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use core::sync::atomic::{AtomicBool, Ordering};
use std::{
    io,
    os::fortanix_sgx::{
        thread::current as current_tcs,
        usercalls::{
            self,
            raw::{Tcs, EV_UNPARK, WAIT_INDEFINITE},
        },
    },
    thread,
    time::Instant,
};

// Helper type for putting a thread to sleep until some other thread wakes it up
pub struct ThreadParker {
    parked: AtomicBool,
    tcs: Tcs,
}

impl ThreadParker {
    pub const IS_CHEAP_TO_CONSTRUCT: bool = true;

    #[inline]
    pub fn new() -> ThreadParker {
        ThreadParker {
            parked: AtomicBool::new(false),
            tcs: current_tcs(),
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    #[inline]
    pub fn prepare_park(&self) {
        self.parked.store(true, Ordering::Relaxed);
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    #[inline]
    pub fn timed_out(&self) -> bool {
        self.parked.load(Ordering::Relaxed)
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    #[inline]
    pub fn park(&self) {
        while self.parked.load(Ordering::Acquire) {
            let result = usercalls::wait(EV_UNPARK, WAIT_INDEFINITE);
            debug_assert_eq!(result.expect("wait returned error") & EV_UNPARK, EV_UNPARK);
        }
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    #[inline]
    pub fn park_until(&self, _timeout: Instant) -> bool {
        // FIXME: https://github.com/fortanix/rust-sgx/issues/31
        panic!("timeout not supported in SGX");
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    #[inline]
    pub fn unpark_lock(&self) -> UnparkHandle {
        // We don't need to lock anything, just clear the state
        self.parked.store(false, Ordering::Release);
        UnparkHandle(self.tcs)
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle(Tcs);

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    #[inline]
    pub fn unpark(self) {
        let result = usercalls::send(EV_UNPARK, Some(self.0));
        if cfg!(debug_assertions) {
            if let Err(error) = result {
                // `InvalidInput` may be returned if the thread we send to has
                // already been unparked and exited.
                if error.kind() != io::ErrorKind::InvalidInput {
                    panic!("send returned an unexpected error: {:?}", error);
                }
            }
        }
    }
}

#[inline]
pub fn thread_yield() {
    thread::yield_now();
}
