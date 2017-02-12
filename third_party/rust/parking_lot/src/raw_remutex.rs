// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::{Duration, Instant};
use std::cell::Cell;
use raw_mutex::RawMutex;
#[cfg(not(target_os = "emscripten"))]
use thread_id;

// Helper function to get a thread id
#[cfg(not(target_os = "emscripten"))]
fn get_thread_id() -> usize {
    thread_id::get()
}
#[cfg(target_os = "emscripten")]
fn get_thread_id() -> usize {
    // pthread_self returns 0 on enscripten, but we use that as a
    // reserved value to indicate an empty slot. We instead fall
    // back to using the address of a thread-local variable, which
    // is slightly slower but guaranteed to produce a non-zero value.
    thread_local!(static KEY: u8 = unsafe { std::mem::uninitialized() });
    KEY.with(|x| x as *const _ as usize)
}

pub struct RawReentrantMutex {
    owner: AtomicUsize,
    lock_count: Cell<usize>,
    mutex: RawMutex,
}

unsafe impl Sync for RawReentrantMutex {}

impl RawReentrantMutex {
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new() -> RawReentrantMutex {
        RawReentrantMutex {
            owner: AtomicUsize::new(0),
            lock_count: Cell::new(0),
            mutex: RawMutex::new(),
        }
    }
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new() -> RawReentrantMutex {
        RawReentrantMutex {
            owner: AtomicUsize::new(0),
            lock_count: Cell::new(0),
            mutex: RawMutex::new(),
        }
    }

    #[inline]
    fn lock_internal<F: FnOnce() -> bool>(&self, try_lock: F) -> bool {
        let id = get_thread_id();
        if self.owner.load(Ordering::Relaxed) == id {
            self.lock_count.set(self.lock_count
                .get()
                .checked_add(1)
                .expect("ReentrantMutex lock count overflow"));
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
    pub fn lock(&self) {
        self.lock_internal(|| {
            self.mutex.lock();
            true
        });
    }

    #[inline]
    pub fn try_lock_until(&self, timeout: Instant) -> bool {
        self.lock_internal(|| self.mutex.try_lock_until(timeout))
    }

    #[inline]
    pub fn try_lock_for(&self, timeout: Duration) -> bool {
        self.lock_internal(|| self.mutex.try_lock_for(timeout))
    }

    #[inline]
    pub fn try_lock(&self) -> bool {
        self.lock_internal(|| self.mutex.try_lock())
    }

    #[inline]
    pub fn unlock(&self, force_fair: bool) {
        let lock_count = self.lock_count.get() - 1;
        if lock_count == 0 {
            self.owner.store(0, Ordering::Relaxed);
            self.mutex.unlock(force_fair);
        } else {
            self.lock_count.set(lock_count);
        }
    }
}
