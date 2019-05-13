// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for NetBSD

use rand_core::Error;
use super::random_device;
use super::OsRngImpl;

use std::fs::File;
use std::io::Read;
use std::sync::atomic::{AtomicBool, Ordering};
#[allow(deprecated)]  // Required for compatibility with Rust < 1.24.
use std::sync::atomic::ATOMIC_BOOL_INIT;

#[derive(Clone, Debug)]
pub struct OsRng { initialized: bool }

impl OsRngImpl for OsRng {
    fn new() -> Result<OsRng, Error> {
        random_device::open("/dev/urandom", &|p| File::open(p))?;
        Ok(OsRng { initialized: false })
    }

    fn fill_chunk(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        random_device::read(dest)
    }

    // Read a single byte from `/dev/random` to determine if the OS RNG is
    // already seeded. NetBSD always blocks if not yet ready.
    fn test_initialized(&mut self, dest: &mut [u8], _blocking: bool)
        -> Result<usize, Error>
    {
        #[allow(deprecated)]
        static OS_RNG_INITIALIZED: AtomicBool = ATOMIC_BOOL_INIT;
        if !self.initialized {
            self.initialized = OS_RNG_INITIALIZED.load(Ordering::Relaxed);
        }
        if self.initialized { return Ok(0); }

        info!("OsRng: testing random device /dev/random");
        let mut file =
            File::open("/dev/random").map_err(random_device::map_err)?;
        file.read(&mut dest[..1]).map_err(random_device::map_err)?;

        OS_RNG_INITIALIZED.store(true, Ordering::Relaxed);
        self.initialized = true;
        Ok(1)
    }

    fn method_str(&self) -> &'static str { "/dev/urandom" }
}
