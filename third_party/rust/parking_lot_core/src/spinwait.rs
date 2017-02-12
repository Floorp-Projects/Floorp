// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[cfg(windows)]
use kernel32;
#[cfg(unix)]
use libc;
#[cfg(not(any(windows, unix)))]
use std::thread;
#[cfg(not(feature = "nightly"))]
use std::sync::atomic::{Ordering, fence};

// Yields the rest of the current timeslice to the OS
#[cfg(windows)]
#[inline]
fn thread_yield() {
    unsafe {
        // We don't use SwitchToThread here because it doesn't consider all
        // threads in the system and the thread we are waiting for may not get
        // selected.
        kernel32::Sleep(0);
    }
}
#[cfg(unix)]
#[inline]
fn thread_yield() {
    unsafe {
        libc::sched_yield();
    }
}
#[cfg(not(any(windows, unix)))]
#[inline]
fn thread_yield() {
    thread::yield_now();
}

// Wastes some CPU time for the given number of iterations, preferably also
// using a hint to indicate to the CPU that we are spinning.
#[cfg(all(feature = "nightly", any(target_arch = "x86", target_arch = "x86_64")))]
#[inline]
fn cpu_relax(iterations: u32) {
    for _ in 0..iterations {
        unsafe {
            asm!("pause" ::: "memory" : "volatile");
        }
    }
}
#[cfg(all(feature = "nightly", target_arch = "aarch64"))]
#[inline]
fn cpu_relax(iterations: u32) {
    for _ in 0..iterations {
        unsafe {
            asm!("yield" ::: "memory" : "volatile");
        }
    }
}
#[cfg(all(feature = "nightly", not(any(target_arch = "x86",
                                       target_arch = "x86_64",
                                       target_arch = "aarch64"))))]
#[inline]
fn cpu_relax(iterations: u32) {
    for _ in 0..iterations {
        unsafe {
            asm!("" ::: "memory" : "volatile");
        }
    }
}
#[cfg(not(feature = "nightly"))]
#[inline]
fn cpu_relax(iterations: u32) {
    // This is a bit tricky: we rely on the fact that LLVM doesn't optimize
    // atomic operations and effectively treats them as volatile.
    for _ in 0..iterations {
        fence(Ordering::SeqCst);
    }
}

/// A counter used to perform exponential backoff in spin loops.
pub struct SpinWait {
    counter: u32,
}

impl SpinWait {
    /// Creates a new `SpinWait`.
    #[cfg(feature = "nightly")]
    #[inline]
    pub const fn new() -> SpinWait {
        SpinWait { counter: 0 }
    }

    /// Creates a new `SpinWait`.
    #[cfg(not(feature = "nightly"))]
    #[inline]
    pub fn new() -> SpinWait {
        SpinWait { counter: 0 }
    }

    /// Resets a `SpinWait` to its initial state.
    #[inline]
    pub fn reset(&mut self) {
        self.counter = 0;
    }

    /// Spins until the sleep threshold has been reached.
    ///
    /// This function returns whether the sleep threshold has been reached, at
    /// which point further spinning has diminishing returns and the thread
    /// should be parked instead.
    ///
    /// The spin strategy will initially use a CPU-bound loop but will fall back
    /// to yielding the CPU to the OS after a few iterations.
    #[inline]
    pub fn spin(&mut self) -> bool {
        if self.counter >= 20 {
            return false;
        }
        self.counter += 1;
        if self.counter <= 10 {
            cpu_relax(4 << self.counter);
        } else {
            thread_yield();
        }
        true
    }

    /// Spins without yielding the thread to the OS.
    ///
    /// Instead, the backoff is simply capped at a maximum value. This can be
    /// used to improve throughput in `compare_exchange` loops that have high
    /// contention.
    #[inline]
    pub fn spin_no_yield(&mut self) {
        self.counter += 1;
        if self.counter > 10 {
            self.counter = 10;
        }
        cpu_relax(4 << self.counter);
    }
}

impl Default for SpinWait {
    #[inline]
    fn default() -> SpinWait {
        SpinWait::new()
    }
}
