// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::ffi::CStr;
use std::os::raw::c_char;
use std::ptr;

pub trait UnwrapCStr {
    fn unwrap_cstr(self) -> *const c_char;
}

impl<'a, U> UnwrapCStr for U
    where U: Into<Option<&'a CStr>>
{
    fn unwrap_cstr(self) -> *const c_char {
        self.into().map(|o| o.as_ptr()).unwrap_or(0 as *const _)
    }
}

pub fn map_to_mut_ptr<T, U, F: FnOnce(&T) -> *mut U>(t: Option<&mut T>, f: F) -> *mut U {
    match t {
        Some(x) => f(x),
        None => ptr::null_mut(),
    }
}

pub fn str_to_ptr(s: Option<&CStr>) -> *const c_char {
    match s {
        Some(x) => x.as_ptr(),
        None => ptr::null(),
    }
}

pub fn to_ptr<T>(t: Option<&T>) -> *const T {
    match t {
        Some(x) => x as *const T,
        None => ptr::null(),
    }
}
