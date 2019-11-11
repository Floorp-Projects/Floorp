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

fn syscall_getrandom(dest: &mut [u8]) -> Result<(), io::Error> {
    let ret = unsafe {
        libc::syscall(libc::SYS_getrandom, dest.as_mut_ptr(), dest.len(), 0)
    };
    if ret < 0 || (ret as usize) != dest.len() {
        error!("Linux getrandom syscall failed with return value {}", ret);
        return Err(io::Error::last_os_error());
    }
    Ok(())
}

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
                RngSource::GetRandom => syscall_getrandom(dest),
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
        let mut buf: [u8; 0] = [];
        let available = match syscall_getrandom(&mut buf) {
            Ok(()) => true,
            Err(err) => err.raw_os_error() != Some(libc::ENOSYS),
        };
        AVAILABLE.store(available, Ordering::Relaxed);
    });

    AVAILABLE.load(Ordering::Relaxed)
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
