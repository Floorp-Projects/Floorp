// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The ISAAC and ISAAC-64 random number generators.

#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://rust-random.github.io/rand/")]

#![deny(missing_docs)]
#![deny(missing_debug_implementations)]
#![doc(test(attr(allow(unused_variables), deny(warnings))))]

#![cfg_attr(not(all(feature="serde1", test)), no_std)]

extern crate rand_core;

#[cfg(feature="serde1")] extern crate serde;
#[cfg(feature="serde1")] #[macro_use] extern crate serde_derive;

// To test serialization we need bincode and the standard library
#[cfg(all(feature="serde1", test))] extern crate bincode;
#[cfg(all(feature="serde1", test))] extern crate std as core;

pub mod isaac;
pub mod isaac64;

mod isaac_array;

pub use self::isaac::IsaacRng;
pub use self::isaac64::Isaac64Rng;
