// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A URL type for Core Foundation.

pub use core_foundation_sys::url::*;

use base::{TCFType};
use string::{CFString};

use core_foundation_sys::base::{kCFAllocatorDefault, CFRelease};
use std::fmt;

pub struct CFURL(CFURLRef);

impl Drop for CFURL {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFURL, CFURLRef, CFURLGetTypeID);

impl fmt::Debug for CFURL {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        unsafe {
            let string: CFString = TCFType::wrap_under_get_rule(CFURLGetString(self.0));
            write!(f, "{}", string.to_string())
        }
    }
}

impl CFURL {
    pub fn from_file_system_path(filePath: CFString, pathStyle: CFURLPathStyle, isDirectory: bool) -> CFURL {
        unsafe {
            let url_ref = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePath.as_concrete_TypeRef(), pathStyle, isDirectory as u8);
            TCFType::wrap_under_create_rule(url_ref)
        }
    }

    pub fn get_string(&self) -> CFString {
        unsafe {
            TCFType::wrap_under_get_rule(CFURLGetString(self.0))
        }
    }
}

#[test]
fn file_url_from_path() {
    let path = "/usr/local/foo/";
    let cfstr_path = CFString::from_static_string(path);
    let cfurl = CFURL::from_file_system_path(cfstr_path, kCFURLPOSIXPathStyle, true);
    assert!(cfurl.get_string().to_string() == "file:///usr/local/foo/");
}
