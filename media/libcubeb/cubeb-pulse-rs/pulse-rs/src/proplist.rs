// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use std::ffi::{CStr, CString};

#[derive(Debug)]
pub struct Proplist(*mut ffi::pa_proplist);

impl Proplist {
    pub fn gets<T>(&self, key: T) -> Option<&CStr>
        where T: Into<Vec<u8>>
    {
        let key = match CString::new(key) {
            Ok(k) => k,
            _ => return None,
        };
        let r = unsafe { ffi::pa_proplist_gets(self.0, key.as_ptr()) };
        if r.is_null() {
            None
        } else {
            Some(unsafe { CStr::from_ptr(r) })
        }
    }
}

pub unsafe fn from_raw_ptr(raw: *mut ffi::pa_proplist) -> Proplist {
    return Proplist(raw);
}
