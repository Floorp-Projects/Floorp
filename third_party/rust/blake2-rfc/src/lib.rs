// Copyright 2015 blake2-rfc Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! A pure Rust implementation of BLAKE2 based on RFC 7693.

#![no_std]

#![warn(missing_docs)]

#![cfg_attr(feature = "clippy", feature(plugin))]
#![cfg_attr(feature = "clippy", plugin(clippy))]
#![cfg_attr(feature = "clippy", warn(clippy_pedantic))]

#![cfg_attr(all(feature = "bench", test), feature(test))]
#![cfg_attr(feature = "simd", feature(platform_intrinsics, repr_simd))]
#![cfg_attr(feature = "simd_opt", feature(cfg_target_feature))]
#![cfg_attr(feature = "simd_asm", feature(asm))]

#[cfg(any(feature = "std", all(feature = "bench", test)))]
#[macro_use]
extern crate std;

#[cfg(all(feature = "bench", test))]
extern crate test;

extern crate arrayvec;
extern crate constant_time_eq;

mod as_bytes;
mod bytes;

mod simdty;
mod simdint;
mod simdop;
mod simd_opt;
mod simd;

#[macro_use]
mod blake2;

pub mod blake2b;
pub mod blake2s;

/// Runs the self-test for both BLAKE2b and BLAKE2s.
#[cold]
pub fn selftest() {
    blake2b::selftest();
    blake2s::selftest();
}
