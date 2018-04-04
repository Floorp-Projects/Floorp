// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![crate_name = "string_cache"]
#![crate_type = "rlib"]

#![cfg_attr(test, deny(warnings))]
#![cfg_attr(all(test, feature = "unstable"), feature(test))]

#[cfg(all(test, feature = "unstable"))] extern crate test;
#[cfg(all(test, feature = "unstable"))] extern crate rand;
#[macro_use] extern crate lazy_static;
#[macro_use] extern crate debug_unreachable;
extern crate phf_shared;
extern crate precomputed_hash;
extern crate serde;
extern crate string_cache_shared as shared;

pub use atom::{Atom, StaticAtomSet, PhfStrSet, EmptyStaticAtomSet, DefaultAtom};

#[cfg(feature = "log-events")]
#[macro_use]
pub mod event;

pub mod atom;

// Make test_atom! macro work in this crate.
// `$crate` would not be appropriate for other crates creating such macros
mod string_cache {
    pub use {Atom, StaticAtomSet, PhfStrSet};
}
