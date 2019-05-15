// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for OpenBSD / Bitrig

extern crate libc;

use rand_core::{Error, ErrorKind};
use super::OsRngImpl;

use std::io;

#[derive(Clone, Debug)]
pub struct OsRng;

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> { Ok(OsRng) }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        let ret = unsafe {
            libc::getentropy(dest.as_mut_ptr() as *mut libc::c_void, dest.len())
        };
        if ret == -1 {
            return Err(Error::with_cause(
                ErrorKind::Unavailable,
                "getentropy failed",
                io::Error::last_os_error()));
        }
        Ok(())
    }

    fn max_chunk_size(&self) -> usize { 256 }

    fn method_str(&self) -> &'static str { "getentropy" }
}
