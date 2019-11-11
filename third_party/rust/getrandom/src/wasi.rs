// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for WASI
use crate::Error;
use core::num::NonZeroU32;
use std::io;

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    let ret = unsafe { 
        libc::__wasi_random_get(dest.as_mut_ptr() as *mut libc::c_void, dest.len())
    };
    if ret == libc::__WASI_ESUCCESS {
        Ok(())
    } else {
        error!("WASI: __wasi_random_get failed with return value {}", ret);
        Err(io::Error::last_os_error().into())
    }
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }