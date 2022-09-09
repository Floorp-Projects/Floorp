// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![macro_use]

use std::os::raw::c_int;
use Error;

pub fn cvt_r(ret: c_int) -> Result<(), Error> {
    match ret {
        n if n < 0 => Err(Error::from_raw(n)),
        _ => Ok(()),
    }
}

macro_rules! call {
    (ffi::$p:ident ($($e:expr),*)) => ({
        ::call::cvt_r(ffi::$p($($e),*))
    })
}
