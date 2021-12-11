// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use core::{
    ptr,
    sync::atomic::{AtomicPtr, AtomicUsize, Ordering},
};
use instant::Instant;

mod keyed_event;
mod waitaddress;

enum Backend {
    KeyedEvent(keyed_event::KeyedEvent),
    WaitAddress(waitaddress::WaitAddress),
}

static BACKEND: AtomicPtr<Backend> = AtomicPtr::new(ptr::null_mut());

impl Backend {
    #[inline]
    fn get() -> &'static Backend {
        // Fast path: use the existing object
        let backend_ptr = BACKEND.load(Ordering::Acquire);
        if !backend_ptr.is_null() {
            return unsafe { &*backend_ptr };
        };

        Backend::create()
    }

    #[cold]
    fn create() -> &'static Backend {
        // Try to create a new Backend
        let backend;
        if let Some(waitaddress) = waitaddress::WaitAddress::create() {
            backend = Backend::WaitAddress(waitaddress);
        } else if let Some(keyed_event) = keyed_event::KeyedEvent::create() {
            backend = Backend::KeyedEvent(keyed_event);
        } else {
            panic!(
                "parking_lot requires either NT Keyed Events (WinXP+) or \
                 WaitOnAddress/WakeByAddress (Win8+)"
            );
        }

        // Try to set our new Backend as the global one
        let backend_ptr = Box::into_raw(Box::new(backend));
        match BACKEND.compare_exchange(
            ptr::null_mut(),
            backend_ptr,
            Ordering::Release,
            Ordering::Relaxed,
        ) {
            Ok(_) => unsafe { &*backend_ptr },
            Err(global_backend_ptr) => {
                unsafe {
                    // We lost the race, free our object and return the global one
                    Box::from_raw(backend_ptr);
                    &*global_backend_ptr
                }
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
    pub const IS_CHEAP_TO_CONSTRUCT: bool = true;

    #[inline]
    pub fn new() -> ThreadParker {
        // Initialize the backend here to ensure we don't get any panics
        // later on, which could leave synchronization primitives in a broken
        // state.
        ThreadParker {
            key: AtomicUsize::new(0),
            backend: Backend::get(),
        }
    }

    // Prepares the parker. This should be called before adding it to the queue.
    #[inline]
    pub fn prepare_park(&self) {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.prepare_park(&self.key),
            Backend::WaitAddress(ref x) => x.prepare_park(&self.key),
        }
    }

    // Checks if the park timed out. This should be called while holding the
    // queue lock after park_until has returned false.
    #[inline]
    pub fn timed_out(&self) -> bool {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.timed_out(&self.key),
            Backend::WaitAddress(ref x) => x.timed_out(&self.key),
        }
    }

    // Parks the thread until it is unparked. This should be called after it has
    // been added to the queue, after unlocking the queue.
    #[inline]
    pub unsafe fn park(&self) {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.park(&self.key),
            Backend::WaitAddress(ref x) => x.park(&self.key),
        }
    }

    // Parks the thread until it is unparked or the timeout is reached. This
    // should be called after it has been added to the queue, after unlocking
    // the queue. Returns true if we were unparked and false if we timed out.
    #[inline]
    pub unsafe fn park_until(&self, timeout: Instant) -> bool {
        match *self.backend {
            Backend::KeyedEvent(ref x) => x.park_until(&self.key, timeout),
            Backend::WaitAddress(ref x) => x.park_until(&self.key, timeout),
        }
    }

    // Locks the parker to prevent the target thread from exiting. This is
    // necessary to ensure that thread-local ThreadData objects remain valid.
    // This should be called while holding the queue lock.
    #[inline]
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
    #[inline]
    pub unsafe fn unpark(self) {
        match self {
            UnparkHandle::KeyedEvent(x) => x.unpark(),
            UnparkHandle::WaitAddress(x) => x.unpark(),
        }
    }
}

// Yields the rest of the current timeslice to the OS
#[inline]
pub fn thread_yield() {
    // Note that this is manually defined here rather than using the definition
    // through `winapi`. The `winapi` definition comes from the `synchapi`
    // header which enables the "synchronization.lib" library. It turns out,
    // however that `Sleep` comes from `kernel32.dll` so this activation isn't
    // necessary.
    //
    // This was originally identified in rust-lang/rust where on MinGW the
    // libsynchronization.a library pulls in a dependency on a newer DLL not
    // present in older versions of Windows. (see rust-lang/rust#49438)
    //
    // This is a bit of a hack for now and ideally we'd fix MinGW's own import
    // libraries, but that'll probably take a lot longer than patching this here
    // and avoiding the `synchapi` feature entirely.
    extern "system" {
        fn Sleep(a: winapi::shared::minwindef::DWORD);
    }
    unsafe {
        // We don't use SwitchToThread here because it doesn't consider all
        // threads in the system and the thread we are waiting for may not get
        // selected.
        Sleep(0);
    }
}
