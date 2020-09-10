/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;

macro_rules! try_or {
    ($val:expr, $or:expr) => {
        match $val {
            Ok(v) => v,
            Err(e) => {
                return $or(e);
            }
        }
    };
}

pub trait Signed {
    fn is_negative(&self) -> bool;
}

impl Signed for i32 {
    fn is_negative(&self) -> bool {
        *self < (0 as i32)
    }
}

impl Signed for usize {
    fn is_negative(&self) -> bool {
        (*self as isize) < (0 as isize)
    }
}

#[cfg(any(target_os = "linux"))]
pub fn from_unix_result<T: Signed>(rv: T) -> io::Result<T> {
    if rv.is_negative() {
        let errno = unsafe { *libc::__errno_location() };
        Err(io::Error::from_raw_os_error(errno))
    } else {
        Ok(rv)
    }
}

#[cfg(any(target_os = "freebsd"))]
pub fn from_unix_result<T: Signed>(rv: T) -> io::Result<T> {
    if rv.is_negative() {
        let errno = unsafe { *libc::__error() };
        Err(io::Error::from_raw_os_error(errno))
    } else {
        Ok(rv)
    }
}

#[cfg(any(target_os = "openbsd"))]
pub fn from_unix_result<T: Signed>(rv: T) -> io::Result<T> {
    if rv.is_negative() {
        Err(io::Error::last_os_error())
    } else {
        Ok(rv)
    }
}

pub fn io_err(msg: &str) -> io::Error {
    io::Error::new(io::ErrorKind::Other, msg)
}
