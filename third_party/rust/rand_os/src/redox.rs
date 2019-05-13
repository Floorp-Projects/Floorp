// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for Redox

use rand_core::Error;
use super::random_device;
use super::OsRngImpl;
use std::fs::File;

#[derive(Clone, Debug)]
pub struct OsRng();

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        random_device::open("rand:", &|p| File::open(p))?;
        Ok(OsRng())
    }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        random_device::read(dest)
    }

    fn method_str(&self) -> &'static str { "'rand:'" }
}
