// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(all(feature = "unstable", test), feature(test))]
#![allow(dead_code)]

#[macro_use]
mod debug;

pub mod constraint;
pub mod graph;
pub mod snapshot_vec;
#[cfg(feature = "unstable")]
pub mod cc;
pub mod unify;
pub mod bitvec;
