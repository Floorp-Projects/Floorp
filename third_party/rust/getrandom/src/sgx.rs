// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for SGX using RDRAND instruction
use crate::Error;
use core::{mem, ptr};
use core::arch::x86_64::_rdrand64_step;
use core::num::NonZeroU32;

#[cfg(not(target_feature = "rdrand"))]
compile_error!("enable rdrand target feature!");

const RETRY_LIMIT: usize = 32;

fn get_rand_u64() -> Result<u64, Error> {
    for _ in 0..RETRY_LIMIT {
        unsafe {
            let mut el = mem::uninitialized();
            if _rdrand64_step(&mut el) == 1 {
                return Ok(el);
            }
        };
    }
    Err(Error::UNKNOWN)
}

pub fn getrandom_inner(mut dest: &mut [u8]) -> Result<(), Error> {
    while dest.len() >= 8 {
        let (chunk, left) = {dest}.split_at_mut(8);
        dest = left;
        let r = get_rand_u64()?;
        unsafe {
            ptr::write_unaligned(chunk.as_mut_ptr() as *mut u64, r)
        }
    }
    let n = dest.len();
    if n != 0 {
        let r = get_rand_u64()?;
        let r: [u8; 8] = unsafe { mem::transmute(r) };
        dest.copy_from_slice(&r[..n]);
    }
    Ok(())
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
