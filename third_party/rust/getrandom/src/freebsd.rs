// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for FreeBSD
extern crate std;

use crate::Error;
use std::io;
use core::ptr;
use core::num::NonZeroU32;

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    let mib = [libc::CTL_KERN, libc::KERN_ARND];
    // kern.arandom permits a maximum buffer size of 256 bytes
    for chunk in dest.chunks_mut(256) {
        let mut len = chunk.len();
        let ret = unsafe {
            libc::sysctl(
                mib.as_ptr(), mib.len() as libc::c_uint,
                chunk.as_mut_ptr() as *mut _, &mut len, ptr::null(), 0,
            )
        };
        if ret == -1 || len != chunk.len() {
            error!("freebsd: kern.arandom syscall failed");
            return Err(io::Error::last_os_error().into());
        }
    }
    Ok(())
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
