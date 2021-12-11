// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for VxWorks
use crate::error::{Error, RAND_SECURE_FATAL};
use crate::util_libc::last_os_error;
use core::sync::atomic::{AtomicBool, Ordering::Relaxed};

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    static RNG_INIT: AtomicBool = AtomicBool::new(false);
    while !RNG_INIT.load(Relaxed) {
        let ret = unsafe { libc::randSecure() };
        if ret < 0 {
            return Err(RAND_SECURE_FATAL);
        } else if ret > 0 {
            RNG_INIT.store(true, Relaxed);
            break;
        }
        unsafe { libc::usleep(10) };
    }

    // Prevent overflow of i32
    for chunk in dest.chunks_mut(i32::max_value() as usize) {
        let ret = unsafe { libc::randABytes(chunk.as_mut_ptr(), chunk.len() as i32) };
        if ret != 0 {
            return Err(last_os_error());
        }
    }
    Ok(())
}
