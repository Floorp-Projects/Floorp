// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for Linux / Android
extern crate std;

use crate::Error;
use crate::utils::use_init;
use std::{thread_local, io::{self, Read}, fs::File};
use core::cell::RefCell;
use core::num::NonZeroU32;
use core::sync::atomic::{AtomicBool, Ordering};

static RNG_INIT: AtomicBool = AtomicBool::new(false);

enum RngSource {
    GetRandom,
    Device(File),
}

thread_local!(
    static RNG_SOURCE: RefCell<Option<RngSource>> = RefCell::new(None);
);

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    RNG_SOURCE.with(|f| {
        use_init(f,
        || {
            let s = if is_getrandom_available() {
                RngSource::GetRandom
            } else {
                // read one byte from "/dev/random" to ensure that
                // OS RNG has initialized
                if !RNG_INIT.load(Ordering::Relaxed) {
                    File::open("/dev/random")?.read_exact(&mut [0u8; 1])?;
                    RNG_INIT.store(true, Ordering::Relaxed)
                }
                RngSource::Device(File::open("/dev/urandom")?)
            };
            Ok(s)
        }, |f| {
            match f {
                RngSource::GetRandom => {
                    sys_fill_exact(dest, |buf| unsafe {
                        getrandom(buf.as_mut_ptr() as *mut libc::c_void, buf.len(), 0)
                    })
                },
                RngSource::Device(f) => f.read_exact(dest),
            }.map_err(From::from)
        })
    })
}

fn is_getrandom_available() -> bool {
    use std::sync::{Once, ONCE_INIT};

    static CHECKER: Once = ONCE_INIT;
    static AVAILABLE: AtomicBool = AtomicBool::new(false);

    CHECKER.call_once(|| {
        let available = {
            let res = unsafe { getrandom(core::ptr::null_mut(), 0, libc::GRND_NONBLOCK) };
            if res < 0 {
                match io::Error::last_os_error().raw_os_error() {
                    Some(libc::ENOSYS) => false, // No kernel support
                    Some(libc::EPERM) => false,  // Blocked by seccomp
                    _ => true,
                }
            } else {
                true
            }
        };
        AVAILABLE.store(available, Ordering::Relaxed);
    });

    AVAILABLE.load(Ordering::Relaxed)
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }

// Fill a buffer by repeatedly invoking a system call. The `sys_fill` function:
//   - should return -1 and set errno on failure
//   - should return the number of bytes written on success
//
// From src/util_libc.rs 65660e00
fn sys_fill_exact(
    mut buf: &mut [u8],
    sys_fill: impl Fn(&mut [u8]) -> libc::ssize_t,
) -> Result<(), io::Error> {
    while !buf.is_empty() {
        let res = sys_fill(buf);
        if res < 0 {
            let err = io::Error::last_os_error();
            // We should try again if the call was interrupted.
            if err.raw_os_error() != Some(libc::EINTR) {
                return Err(err.into());
            }
        } else {
            // We don't check for EOF (ret = 0) as the data we are reading
            // should be an infinite stream of random bytes.
            buf = &mut buf[(res as usize)..];
        }
    }
    Ok(())
}

unsafe fn getrandom(
    buf: *mut libc::c_void,
    buflen: libc::size_t,
    flags: libc::c_uint,
) -> libc::ssize_t {
    libc::syscall(libc::SYS_getrandom, buf, buflen, flags) as libc::ssize_t
}
