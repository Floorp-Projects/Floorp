// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for CloudABI

extern crate cloudabi;

use std::io;
use rand_core::{Error, ErrorKind};
use super::OsRngImpl;

#[derive(Clone, Debug)]
pub struct OsRng;

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> { Ok(OsRng) }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        let errno = unsafe { cloudabi::random_get(dest) };
        if errno == cloudabi::errno::SUCCESS {
            Ok(())
        } else {
            // Cloudlibc provides its own `strerror` implementation so we
            // can use `from_raw_os_error` here.
            Err(Error::with_cause(
                ErrorKind::Unavailable,
                "random_get() system call failed",
                io::Error::from_raw_os_error(errno as i32),
            ))
        }
    }

    fn method_str(&self) -> &'static str { "cloudabi::random_get" }
}
