//! Cubeb backend interface to Pulse Audio

// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#[macro_use]
extern crate cubeb_ffi as cubeb;
extern crate pulse_ffi;
extern crate pulse;
extern crate semver;

mod capi;
mod backend;

pub use capi::pulse_rust_init;
