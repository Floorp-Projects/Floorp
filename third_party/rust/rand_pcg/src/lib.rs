// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The PCG random number generators.
//!
//! This is a native Rust implementation of a small selection of PCG generators.
//! The primary goal of this crate is simple, minimal, well-tested code; in
//! other words it is explicitly not a goal to re-implement all of PCG.
//!
//! This crate provides:
//!
//! -   `Pcg32` aka `Lcg64Xsh32`, officially known as `pcg32`, a general
//!     purpose RNG. This is a good choice on both 32-bit and 64-bit CPUs
//!     (for 32-bit output).
//! -   `Pcg64Mcg` aka `Mcg128Xsl64`, officially known as `mcg_xsl_rr_128_64`,
//!     a general purpose RNG using 128-bit multiplications. This has poor
//!     performance on 32-bit CPUs but is a good choice on 64-bit CPUs for
//!     both 32-bit and 64-bit output. (Note: this RNG is only available using
//!     Rust 1.26 or later.)
//!
//! Both of these use 16 bytes of state and 128-bit seeds, and are considered
//! value-stable (i.e. any change affecting the output given a fixed seed would
//! be considered a breaking change to the crate).

#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://rust-random.github.io/rand/")]

#![deny(missing_docs)]
#![deny(missing_debug_implementations)]

#![no_std]

pub extern crate rand_core;

#[cfg(feature="serde1")] extern crate serde;
#[cfg(feature="serde1")] #[macro_use] extern crate serde_derive;

mod pcg64;
#[cfg(all(rustc_1_26, not(target_os = "emscripten")))] mod pcg128;

pub use self::pcg64::{Pcg32, Lcg64Xsh32};
#[cfg(all(rustc_1_26, not(target_os = "emscripten")))] pub use self::pcg128::{Pcg64Mcg, Mcg128Xsl64};
