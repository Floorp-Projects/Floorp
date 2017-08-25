// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use std::sync::{Mutex, MutexGuard, Condvar};
use std::cell::Cell;
use std::time::Instant;

// Helper type for putting a thread to sleep until some other thread wakes it up
pub struct ThreadParker {
    should_park: Cell<bool>,
    mutex: Mutex<()>,
    condvar: Condvar,
}

impl ThreadParker {
    pub fn new() -> ThreadParker {
        ThreadParker {
            should_park: Cell::new(false),
            mutex: Mutex::new(()),
            condvar: Condvar::new(),
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    pub unsafe fn prepare_park(&self) {
        self.should_park.set(true);
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    pub unsafe fn timed_out(&self) -> bool {
        // We need to grab the mutex here because another thread may be
        // concurrently executing UnparkHandle::unpark, which is done without
        // holding the queue lock.
        let _lock = self.mutex.lock().unwrap();
        self.should_park.get()
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    pub unsafe fn park(&self) {
        let mut lock = self.mutex.lock().unwrap();
        while self.should_park.get() {
            lock = self.condvar.wait(lock).unwrap();
        }
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    pub unsafe fn park_until(&self, timeout: Instant) -> bool {
        let mut lock = self.mutex.lock().unwrap();
        while self.should_park.get() {
            let now = Instant::now();
            if timeout <= now {
                return false;
            }
            let (new_lock, _) = self.condvar.wait_timeout(lock, timeout - now).unwrap();
            lock = new_lock;
        }
        true
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    pub unsafe fn unpark_lock(&self) -> UnparkHandle {
        UnparkHandle {
            thread_parker: self,
            _guard: self.mutex.lock().unwrap(),
        }
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub struct UnparkHandle<'a> {
    thread_parker: *const ThreadParker,
    _guard: MutexGuard<'a, ()>,
}

impl<'a> UnparkHandle<'a> {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    pub unsafe fn unpark(self) {
        (*self.thread_parker).should_park.set(false);

        // We notify while holding the lock here to avoid races with the target
        // thread. In particular, the thread could exit after we unlock the
        // mutex, which would make the condvar access invalid memory.
        (*self.thread_parker).condvar.notify_one();
    }
}
