// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![crate_name = "freetype"]
#![crate_type = "lib"]
#![crate_type = "dylib"]
#![crate_type = "rlib"]

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]

#[cfg(feature = "servo-freetype-sys")]
extern crate freetype_sys;
extern crate libc;

/// A wrapper over FT_Error so we can add convenience methods on it.
#[repr(C)]
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct FT_Error(pub ::std::os::raw::c_int);

impl FT_Error {
    #[inline]
    pub fn succeeded(&self) -> bool {
        self.0 == freetype::FT_Err_Ok as ::std::os::raw::c_int
    }
}

#[allow(improper_ctypes)] // https://github.com/rust-lang/rust/issues/34798
pub mod freetype;
pub mod tt_os2;
