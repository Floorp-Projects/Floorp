// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementations that just need to read from a file
use crate::util::LazyUsize;
use crate::util_libc::{open_readonly, sys_fill_exact};
use crate::Error;
use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicUsize, Ordering::Relaxed};

#[cfg(target_os = "redox")]
const FILE_PATH: &str = "rand:\0";
#[cfg(any(
    target_os = "dragonfly",
    target_os = "emscripten",
    target_os = "haiku",
    target_os = "macos",
    target_os = "solaris",
    target_os = "illumos"
))]
const FILE_PATH: &str = "/dev/random\0";
#[cfg(any(target_os = "android", target_os = "linux"))]
const FILE_PATH: &str = "/dev/urandom\0";

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    let fd = get_rng_fd()?;
    let read = |buf: &mut [u8]| unsafe { libc::read(fd, buf.as_mut_ptr() as *mut _, buf.len()) };

    if cfg!(target_os = "emscripten") {
        // `Crypto.getRandomValues` documents `dest` should be at most 65536 bytes.
        for chunk in dest.chunks_mut(65536) {
            sys_fill_exact(chunk, read)?;
        }
    } else {
        sys_fill_exact(dest, read)?;
    }
    Ok(())
}

// Returns the file descriptor for the device file used to retrieve random
// bytes. The file will be opened exactly once. All successful calls will
// return the same file descriptor. This file descriptor is never closed.
fn get_rng_fd() -> Result<libc::c_int, Error> {
    static FD: AtomicUsize = AtomicUsize::new(LazyUsize::UNINIT);
    fn get_fd() -> Option<libc::c_int> {
        match FD.load(Relaxed) {
            LazyUsize::UNINIT => None,
            val => Some(val as libc::c_int),
        }
    }

    // Use double-checked locking to avoid acquiring the lock if possible.
    if let Some(fd) = get_fd() {
        return Ok(fd);
    }

    // SAFETY: We use the mutex only in this method, and we always unlock it
    // before returning, making sure we don't violate the pthread_mutex_t API.
    static MUTEX: Mutex = Mutex::new();
    unsafe { MUTEX.lock() };
    let _guard = DropGuard(|| unsafe { MUTEX.unlock() });

    if let Some(fd) = get_fd() {
        return Ok(fd);
    }

    // On Linux, /dev/urandom might return insecure values.
    #[cfg(any(target_os = "android", target_os = "linux"))]
    wait_until_rng_ready()?;

    let fd = unsafe { open_readonly(FILE_PATH)? };
    // The fd always fits in a usize without conflicting with UNINIT.
    debug_assert!(fd >= 0 && (fd as usize) < LazyUsize::UNINIT);
    FD.store(fd as usize, Relaxed);

    Ok(fd)
}

// Succeeds once /dev/urandom is safe to read from
#[cfg(any(target_os = "android", target_os = "linux"))]
fn wait_until_rng_ready() -> Result<(), Error> {
    // Poll /dev/random to make sure it is ok to read from /dev/urandom.
    let fd = unsafe { open_readonly("/dev/random\0")? };
    let mut pfd = libc::pollfd {
        fd,
        events: libc::POLLIN,
        revents: 0,
    };
    let _guard = DropGuard(|| unsafe {
        libc::close(fd);
    });

    loop {
        // A negative timeout means an infinite timeout.
        let res = unsafe { libc::poll(&mut pfd, 1, -1) };
        if res >= 0 {
            assert_eq!(res, 1); // We only used one fd, and cannot timeout.
            return Ok(());
        }
        let err = crate::util_libc::last_os_error();
        match err.raw_os_error() {
            Some(libc::EINTR) | Some(libc::EAGAIN) => continue,
            _ => return Err(err),
        }
    }
}

struct Mutex(UnsafeCell<libc::pthread_mutex_t>);

impl Mutex {
    const fn new() -> Self {
        Self(UnsafeCell::new(libc::PTHREAD_MUTEX_INITIALIZER))
    }
    unsafe fn lock(&self) {
        let r = libc::pthread_mutex_lock(self.0.get());
        debug_assert_eq!(r, 0);
    }
    unsafe fn unlock(&self) {
        let r = libc::pthread_mutex_unlock(self.0.get());
        debug_assert_eq!(r, 0);
    }
}

unsafe impl Sync for Mutex {}

struct DropGuard<F: FnMut()>(F);

impl<F: FnMut()> Drop for DropGuard<F> {
    fn drop(&mut self) {
        self.0()
    }
}
