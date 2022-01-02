// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![macro_use]

use Error;
use std::os::raw::c_int;

pub fn cvt_r(ret: c_int) -> Result<(), Error> {
    match ret {
        n if n < 0 => Err(unsafe { Error::from_raw(n) }),
        _ => Ok(()),
    }
}

macro_rules! call {
    (ffi::$p:ident ($($e:expr),*)) => ({
        ::try_call::cvt_r(ffi::$p($($e),*))
    })
}

macro_rules! try_call {
    (ffi::$p:ident ($($e:expr),*)) => ({
        ::try_call::cvt_r(ffi::$p($($e),*))?
    })
}
