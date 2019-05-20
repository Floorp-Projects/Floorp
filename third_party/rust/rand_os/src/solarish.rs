// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for the Solaris family
//!
//! Read from `/dev/random`, with chunks of limited size (1040 bytes).
//! `/dev/random` uses the Hash_DRBG with SHA512 algorithm from NIST SP 800-90A.
//! `/dev/urandom` uses the FIPS 186-2 algorithm, which is considered less
//! secure. We choose to read from `/dev/random`.
//!
//! Since Solaris 11.3 the `getrandom` syscall is available. To make sure we can
//! compile on both Solaris and on OpenSolaris derivatives, that do not have the
//! function, we do a direct syscall instead of calling a library function.
//!
//! We have no way to differentiate between Solaris, illumos, SmartOS, etc.
extern crate libc;

use rand_core::{Error, ErrorKind};
use super::random_device;
use super::OsRngImpl;

use std::io;
use std::io::Read;
use std::fs::{File, OpenOptions};
use std::os::unix::fs::OpenOptionsExt;
use std::sync::atomic::{AtomicBool, Ordering, AtomicUsize};
#[allow(deprecated)]  // Required for compatibility with Rust < 1.24.
use std::sync::atomic::ATOMIC_BOOL_INIT;
use std::cmp;
use std::mem;

#[derive(Clone, Debug)]
pub struct OsRng {
    method: OsRngMethod,
    initialized: bool,
}

#[derive(Clone, Debug)]
enum OsRngMethod {
    GetRandom,
    RandomDevice,
}

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        if is_getrandom_available() {
            return Ok(OsRng { method: OsRngMethod::GetRandom,
                              initialized: false });
        }
        let open = |p| OpenOptions::new()
            .read(true)
            .custom_flags(libc::O_NONBLOCK)
            .open(p);
        random_device::open("/dev/random", &open)?;
        Ok(OsRng { method: OsRngMethod::RandomDevice, initialized: false })
    }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        match self.method {
            OsRngMethod::GetRandom => getrandom_try_fill(dest, false),
            OsRngMethod::RandomDevice => random_device::read(dest),
        }
    }

    fn test_initialized(&mut self, dest: &mut [u8], blocking: bool)
        -> Result<usize, Error>
    {
        #[allow(deprecated)]
        static OS_RNG_INITIALIZED: AtomicBool = ATOMIC_BOOL_INIT;
        if !self.initialized {
            self.initialized = OS_RNG_INITIALIZED.load(Ordering::Relaxed);
        }
        if self.initialized { return Ok(0); }

        let chunk_len = cmp::min(1024, dest.len());
        let dest = &mut dest[..chunk_len];

        match self.method {
            OsRngMethod::GetRandom => getrandom_try_fill(dest, blocking)?,
            OsRngMethod::RandomDevice => {
                if blocking {
                    info!("OsRng: testing random device /dev/random");
                    // We already have a non-blocking handle, but now need a
                    // blocking one. Not much choice except opening it twice
                    let mut file = File::open("/dev/random")
                        .map_err(random_device::map_err)?;
                    file.read(dest).map_err(random_device::map_err)?;
                } else {
                    self.fill_chunk(dest)?;
                }
            }
        };
        OS_RNG_INITIALIZED.store(true, Ordering::Relaxed);
        self.initialized = true;
        Ok(chunk_len)
    }

    fn max_chunk_size(&self) -> usize {
        // This is the largest size that's guaranteed to not block across
        // all the Solarish platforms, though some may allow for larger
        // sizes.
        256
    }

    fn method_str(&self) -> &'static str {
        match self.method {
            OsRngMethod::GetRandom => "getrandom",
            OsRngMethod::RandomDevice => "/dev/random",
        }
    }
}

#[cfg(target_os = "illumos")]
type GetRandomFn = unsafe extern fn(*mut u8, libc::size_t, libc::c_uint)
    -> libc::ssize_t;
#[cfg(target_os = "solaris")]
type GetRandomFn = unsafe extern fn(*mut u8, libc::size_t, libc::c_uint)
    -> libc::c_int;

// Use dlsym to determine if getrandom(2) is present in libc.  On Solarish
// systems, the syscall interface is not stable and can change between
// updates.  Even worse, issuing unsupported syscalls will cause the system
// to generate a SIGSYS signal (usually terminating the program).
// Instead the stable APIs are exposed via libc.  Cache the result of the
// lookup for future calls.  This is loosely modeled after the
// libstd::sys::unix::weak macro which unfortunately is not exported.
fn fetch() -> Option<GetRandomFn> {
    static FPTR: AtomicUsize = AtomicUsize::new(1);

    if FPTR.load(Ordering::SeqCst) == 1 {
        let name = "getrandom\0";
        let addr = unsafe {
            libc::dlsym(libc::RTLD_DEFAULT, name.as_ptr() as *const _) as usize
        };
        FPTR.store(addr, Ordering::SeqCst);
    }

    let ptr = FPTR.load(Ordering::SeqCst);
    unsafe {
        mem::transmute::<usize, Option<GetRandomFn>>(ptr)
    }
}

fn getrandom(buf: &mut [u8], blocking: bool) -> libc::ssize_t {
    const GRND_NONBLOCK: libc::c_uint = 0x0001;
    const GRND_RANDOM: libc::c_uint = 0x0002;

    if let Some(rand) = fetch() {
        let flag = if blocking { 0 } else { GRND_NONBLOCK } | GRND_RANDOM;
        unsafe {
            rand(buf.as_mut_ptr(), buf.len(), flag) as libc::ssize_t
        }
    } else {
        -1
    }
}

fn getrandom_try_fill(dest: &mut [u8], blocking: bool) -> Result<(), Error> {
    let result = getrandom(dest, blocking);
    if result == -1 || result == 0 {
        let err = io::Error::last_os_error();
        let kind = err.kind();
        if kind == io::ErrorKind::WouldBlock {
            return Err(Error::with_cause(
                ErrorKind::NotReady,
                "getrandom not ready",
                err,
            ));
        } else {
            return Err(Error::with_cause(
                ErrorKind::Unavailable,
                "unexpected getrandom error",
                err,
            ));
        }
    } else if result != dest.len() as libc::ssize_t {
        return Err(Error::new(ErrorKind::Unavailable,
                              "unexpected getrandom error"));
    }
    Ok(())
}

fn is_getrandom_available() -> bool {
    let available = match fetch() {
        Some(_) => true,
        None => false,
    };
    info!("OsRng: using {}", if available { "getrandom" } else { "/dev/random" });
    available
}
