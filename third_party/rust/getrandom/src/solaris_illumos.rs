// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for the Solaris family
//!
//! Read from `/dev/random`, with chunks of limited size (256 bytes).
//! `/dev/random` uses the Hash_DRBG with SHA512 algorithm from NIST SP 800-90A.
//! `/dev/urandom` uses the FIPS 186-2 algorithm, which is considered less
//! secure. We choose to read from `/dev/random`.
//!
//! Since Solaris 11.3 and mid-2015 illumos, the `getrandom` syscall is available.
//! To make sure we can compile on both Solaris and its derivatives, as well as
//! function, we check for the existance of getrandom(2) in libc by calling
//! libc::dlsym.
extern crate std;

use crate::Error;
use crate::utils::use_init;
use std::{thread_local, io::{self, Read}, fs::File};
use core::cell::RefCell;
use core::num::NonZeroU32;

#[cfg(target_os = "illumos")]
type GetRandomFn = unsafe extern "C" fn(*mut u8, libc::size_t, libc::c_uint) -> libc::ssize_t;
#[cfg(target_os = "solaris")]
type GetRandomFn = unsafe extern "C" fn(*mut u8, libc::size_t, libc::c_uint) -> libc::c_int;

enum RngSource {
    GetRandom(GetRandomFn),
    Device(File),
}

thread_local!(
    static RNG_SOURCE: RefCell<Option<RngSource>> = RefCell::new(None);
);

fn libc_getrandom(rand: GetRandomFn, dest: &mut [u8]) -> Result<(), Error> {
    let ret = unsafe { rand(dest.as_mut_ptr(), dest.len(), 0) as libc::ssize_t };

    if ret == -1 || ret != dest.len() as libc::ssize_t {
        error!("getrandom syscall failed with ret={}", ret);
        Err(io::Error::last_os_error().into())
    } else {
        Ok(())
    }
}

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    // 256 bytes is the lowest common denominator across all the Solaris
    // derived platforms for atomically obtaining random data.
    RNG_SOURCE.with(|f| {
        use_init(
            f,
            || {
                let s = match fetch_getrandom() {
                    Some(fptr) => RngSource::GetRandom(fptr),
                    None => RngSource::Device(File::open("/dev/random")?),
                };
                Ok(s)
            },
            |f| {
                match f {
                    RngSource::GetRandom(rp) => {
                        for chunk in dest.chunks_mut(256) {
                            libc_getrandom(*rp, chunk)?
                        }
                    }
                    RngSource::Device(randf) => {
                        for chunk in dest.chunks_mut(256) {
                            randf.read_exact(chunk)?
                        }
                    }
                };
                Ok(())
            },
        )
    })
}

fn fetch_getrandom() -> Option<GetRandomFn> {
    use std::mem;
    use std::sync::atomic::{AtomicUsize, Ordering};

    static FPTR: AtomicUsize = AtomicUsize::new(1);

    if FPTR.load(Ordering::SeqCst) == 1 {
        let name = "getrandom\0";
        let addr = unsafe { libc::dlsym(libc::RTLD_DEFAULT, name.as_ptr() as *const _) as usize };
        FPTR.store(addr, Ordering::SeqCst);
    }

    let ptr = FPTR.load(Ordering::SeqCst);
    unsafe { mem::transmute::<usize, Option<GetRandomFn>>(ptr) }
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
