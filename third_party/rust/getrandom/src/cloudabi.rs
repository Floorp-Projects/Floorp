// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for CloudABI
use core::num::NonZeroU32;
use crate::Error;

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    let errno = unsafe { cloudabi::random_get(dest) };
    if errno == cloudabi::errno::SUCCESS {
        Ok(())
    } else {
        let code = NonZeroU32::new(errno as u32).unwrap();
        error!("cloudabi::random_get syscall failed with code {}", code);
        Err(Error::from(code))
    }
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
