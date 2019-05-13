// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for FreeBSD

extern crate libc;

use rand_core::{Error, ErrorKind};
use super::OsRngImpl;

use std::ptr;
use std::io;

#[derive(Clone, Debug)]
pub struct OsRng;

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> { Ok(OsRng) }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        let mib = [libc::CTL_KERN, libc::KERN_ARND];
        let mut len = dest.len();
        let ret = unsafe {
            libc::sysctl(mib.as_ptr(), mib.len() as libc::c_uint,
                         dest.as_mut_ptr() as *mut _, &mut len,
                         ptr::null(), 0)
        };
        if ret == -1 || len != dest.len() {
            return Err(Error::with_cause(
                ErrorKind::Unavailable,
                "kern.arandom sysctl failed",
                io::Error::last_os_error()));
        }
        Ok(())
    }

    fn max_chunk_size(&self) -> usize { 256 }

    fn method_str(&self) -> &'static str { "kern.arandom" }
}
