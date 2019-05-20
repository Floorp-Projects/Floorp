// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::OsRngImpl;
use Error;
use rdrand::RdRand;
use rand_core::RngCore;
use std::fmt::{Debug, Formatter, Result as FmtResult};

#[derive(Clone)]
pub struct OsRng{
    gen: RdRand
}

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        let rng = RdRand::new()?;
        Ok(OsRng{ gen: rng })
    }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.gen.try_fill_bytes(dest)
    }

    fn method_str(&self) -> &'static str { "RDRAND" }
}

impl Debug for OsRng {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        f.debug_struct("OsRng")
            .finish()
    }
}
