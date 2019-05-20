// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for Linux / Android

extern crate libc;

use rand_core::{Error, ErrorKind};
use super::random_device;
use super::OsRngImpl;

use std::io;
use std::io::Read;
use std::fs::{File, OpenOptions};
use std::os::unix::fs::OpenOptionsExt;
use std::sync::atomic::{AtomicBool, Ordering};
#[allow(deprecated)]  // Required for compatibility with Rust < 1.24.
use std::sync::atomic::ATOMIC_BOOL_INIT;
use std::sync::{Once, ONCE_INIT};

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
        random_device::open("/dev/urandom", &|p| File::open(p))?;
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

        let result = match self.method {
            OsRngMethod::GetRandom => {
                getrandom_try_fill(dest, blocking)?;
                Ok(dest.len())
            }
            OsRngMethod::RandomDevice => {
                info!("OsRng: testing random device /dev/random");
                let mut file = OpenOptions::new()
                    .read(true)
                    .custom_flags(if blocking { 0 } else { libc::O_NONBLOCK })
                    .open("/dev/random")
                    .map_err(random_device::map_err)?;
                file.read(&mut dest[..1]).map_err(random_device::map_err)?;
                Ok(1)
            }
        };
        OS_RNG_INITIALIZED.store(true, Ordering::Relaxed);
        self.initialized = true;
        result
    }

    fn method_str(&self) -> &'static str {
        match self.method {
            OsRngMethod::GetRandom => "getrandom",
            OsRngMethod::RandomDevice => "/dev/urandom",
        }
    }
}

#[cfg(target_arch = "x86_64")]
const NR_GETRANDOM: libc::c_long = 318;
#[cfg(target_arch = "x86")]
const NR_GETRANDOM: libc::c_long = 355;
#[cfg(target_arch = "arm")]
const NR_GETRANDOM: libc::c_long = 384;
#[cfg(target_arch = "aarch64")]
const NR_GETRANDOM: libc::c_long = 278;
 #[cfg(target_arch = "s390x")]
const NR_GETRANDOM: libc::c_long = 349;
#[cfg(target_arch = "powerpc")]
const NR_GETRANDOM: libc::c_long = 359;
#[cfg(target_arch = "powerpc64")]
const NR_GETRANDOM: libc::c_long = 359;
#[cfg(target_arch = "mips")] // old ABI
const NR_GETRANDOM: libc::c_long = 4353;
#[cfg(target_arch = "mips64")]
const NR_GETRANDOM: libc::c_long = 5313;
#[cfg(target_arch = "sparc")]
const NR_GETRANDOM: libc::c_long = 347;
#[cfg(target_arch = "sparc64")]
const NR_GETRANDOM: libc::c_long = 347;
#[cfg(not(any(target_arch = "x86_64", target_arch = "x86",
              target_arch = "arm", target_arch = "aarch64",
              target_arch = "s390x", target_arch = "powerpc",
              target_arch = "powerpc64", target_arch = "mips",
              target_arch = "mips64", target_arch = "sparc",
              target_arch = "sparc64")))]
const NR_GETRANDOM: libc::c_long = 0;

fn getrandom(buf: &mut [u8], blocking: bool) -> libc::c_long {
    const GRND_NONBLOCK: libc::c_uint = 0x0001;

    if NR_GETRANDOM == 0 { return -1 };

    unsafe {
        libc::syscall(NR_GETRANDOM, buf.as_mut_ptr(), buf.len(),
                      if blocking { 0 } else { GRND_NONBLOCK })
    }
}

fn getrandom_try_fill(dest: &mut [u8], blocking: bool) -> Result<(), Error> {
    let mut read = 0;
    while read < dest.len() {
        let result = getrandom(&mut dest[read..], blocking);
        if result == -1 {
            let err = io::Error::last_os_error();
            let kind = err.kind();
            if kind == io::ErrorKind::Interrupted {
                continue;
            } else if kind == io::ErrorKind::WouldBlock {
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
        } else {
            read += result as usize;
        }
    }
    Ok(())
}

fn is_getrandom_available() -> bool {
    static CHECKER: Once = ONCE_INIT;
    #[allow(deprecated)]
    static AVAILABLE: AtomicBool = ATOMIC_BOOL_INIT;

    if NR_GETRANDOM == 0 { return false };

    CHECKER.call_once(|| {
        debug!("OsRng: testing getrandom");
        let mut buf: [u8; 0] = [];
        let result = getrandom(&mut buf, false);
        let available = if result == -1 {
            let err = io::Error::last_os_error().raw_os_error();
            err != Some(libc::ENOSYS)
        } else {
            true
        };
        AVAILABLE.store(available, Ordering::Relaxed);
        info!("OsRng: using {}", if available { "getrandom" } else { "/dev/urandom" });
    });

    AVAILABLE.load(Ordering::Relaxed)
}
