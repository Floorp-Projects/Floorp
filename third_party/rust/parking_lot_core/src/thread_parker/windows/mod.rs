// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(feature = "nightly")]
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
#[cfg(not(feature = "nightly"))]
use stable::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
use std::time::Instant;

mod keyed_event;
mod waitaddress;

enum Backend {
    KeyedEvent(keyed_event::KeyedEvent),
    WaitAddress(waitaddress::WaitAddress),
}

impl Backend {
    unsafe fn get() -> &'static Backend {
        static BACKEND: AtomicUsize = ATOMIC_USIZE_INIT;

        // Fast path: use the existing object
        let backend = BACKEND.load(Ordering::Acquire);
        if backend != 0 {
            return &*(backend as *const Backend);
        };

        // Try to create a new Backend
        let backend;
        if let Some(waitaddress) = waitaddress::WaitAddress::create() {
            backend = Backend::WaitAddress(waitaddress);
        } else if let Some(keyed_event) = keyed_event::KeyedEvent::create() {
            backend = Backend::KeyedEvent(keyed_event);
        } else {
            panic!("parking_lot requires either NT Keyed Events (WinXP+) or \
                    WaitOnAddress/WakeByAddress (Win8+)");
        }

        // Try to create a new object
        let backend = Box::into_raw(Box::new(backend));
        match BACKEND.compare_exchange(0, backend as usize, Ordering::Release, Ordering::Relaxed) {
            Ok(_) => &*(backend as *const Backend),
            Err(x) => {
                // We lost the race, free our object and return the global one
                Box::from_raw(backend);
                &*(x as *const Backend)
            }
        }
    }
}

// Helper type for putting a thread to sleep until some other thread wakes it up
pub struct ThreadParker {
    key: AtomicUsize,
    backend: &'static Backend,
}

impl ThreadParker {
    pub fn new() -> ThreadParker {
        // Initialize the backend here to ensure we don't get any panics
        // later on, which could leave synchronization primitives in a broken
        // state.
        ThreadParker {
            key: AtomicUsize::new(0),
            backend: unsafe { Backend::get() },
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    pub unsafe fn prepare_park(&self) {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.prepare_park(&self.key),
            Backend::WaitAddress(ref x) => x.prepare_park(&self.key),
        }
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    pub unsafe fn timed_out(&self) -> bool {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.timed_out(&self.key),
            Backend::WaitAddress(ref x) => x.timed_out(&self.key),
        }
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    pub unsafe fn park(&self) {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.park(&self.key),
            Backend::WaitAddress(ref x) => x.park(&self.key),
        }
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    pub unsafe fn park_until(&self, timeout: Instant) -> bool {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.park_until(&self.key, timeout),
            Backend::WaitAddress(ref x) => x.park_until(&self.key, timeout),
        }
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    pub unsafe fn unpark_lock(&self) -> UnparkHandle {
        match *self.backend {
            Backend::KeyedEvent(ref x) => UnparkHandle::KeyedEvent(x.unpark_lock(&self.key)),
            Backend::WaitAddress(ref x) => UnparkHandle::WaitAddress(x.unpark_lock(&self.key)),
        }
    }
}

// Handle for a thread that is about to be unparked. We need to mark the thread
// as unparked while holding the queue lock, but we delay the actual unparking
// until after the queue lock is released.
pub enum UnparkHandle {
    KeyedEvent(keyed_event::UnparkHandle),
    WaitAddress(waitaddress::UnparkHandle),
}

impl UnparkHandle {
    // Wakes up the parked thread. This should be called after the queue lock is
    // released to avoid blocking the queue for too long.
    pub unsafe fn unpark(self) {
        match self {
            UnparkHandle::KeyedEvent(x) => x.unpark(),
            UnparkHandle::WaitAddress(x) => x.unpark(),
        }
    }
}
