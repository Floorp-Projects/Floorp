// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Implementation for DragonFly / Haiku
extern crate std;

use crate::Error;
use crate::utils::use_init;
use std::{thread_local, io::Read, fs::File};
use core::cell::RefCell;
use core::num::NonZeroU32;

thread_local!(static RNG_FILE: RefCell<Option<File>> = RefCell::new(None));

#[cfg(target_os = "redox")]
const FILE_PATH: &str = "rand:";
#[cfg(target_os = "netbsd")]
const FILE_PATH: &str = "/dev/urandom";
#[cfg(any(target_os = "dragonfly", target_os = "emscripten", target_os = "haiku"))]
const FILE_PATH: &str = "/dev/random";

pub fn getrandom_inner(dest: &mut [u8]) -> Result<(), Error> {
    RNG_FILE.with(|f| {
        use_init(f, || init_file(), |f| use_file(f, dest))
    })
}

fn use_file(f: &mut File, dest: &mut [u8]) -> Result<(), Error> {
    if cfg!(target_os = "emscripten") {
        // `Crypto.getRandomValues` documents `dest` should be at most 65536 bytes.
        for chunk in dest.chunks_mut(65536) {
            f.read_exact(chunk)?;
        }
    } else {
        f.read_exact(dest)?;
    }
    core::mem::forget(f);
    Ok(())
}

fn init_file() -> Result<File, Error> {
    if cfg!(target_os = "netbsd") {
        // read one byte from "/dev/random" to ensure that OS RNG has initialized
        File::open("/dev/random")?.read_exact(&mut [0u8; 1])?;
    }
    let f = File::open(FILE_PATH)?;
    Ok(f)
}

#[inline(always)]
pub fn error_msg_inner(_: NonZeroU32) -> Option<&'static str> { None }
