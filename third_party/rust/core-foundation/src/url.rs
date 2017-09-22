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

use base::{TCFType, CFIndex};
use string::{CFString};

use core_foundation_sys::base::{kCFAllocatorDefault, CFRelease};
use std::fmt;
use std::ptr;
use std::path::Path;

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
    pub fn from_path<P: AsRef<Path>>(path: P, isDirectory: bool) -> Option<CFURL> {
        let path = match path.as_ref().to_str() {
            Some(path) => path,
            None => return None,
        };

        unsafe {
            let url_ref = CFURLCreateFromFileSystemRepresentation(ptr::null_mut(), path.as_ptr(), path.len() as CFIndex, isDirectory as u8);
            if url_ref.is_null() {
                return None;
            }
            Some(TCFType::wrap_under_create_rule(url_ref))
        }
    }

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

    pub fn get_file_system_path(&self, pathStyle: CFURLPathStyle) -> CFString {
        unsafe {
            TCFType::wrap_under_create_rule(CFURLCopyFileSystemPath(self.as_concrete_TypeRef(), pathStyle))
        }
    }

    pub fn absolute(&self) -> CFURL {
        unsafe {
            TCFType::wrap_under_create_rule(CFURLCopyAbsoluteURL(self.as_concrete_TypeRef()))
        }
    }
}

#[test]
fn file_url_from_path() {
    let path = "/usr/local/foo/";
    let cfstr_path = CFString::from_static_string(path);
    let cfurl = CFURL::from_file_system_path(cfstr_path, kCFURLPOSIXPathStyle, true);
    assert_eq!(cfurl.get_string().to_string(), "file:///usr/local/foo/");
}

#[test]
fn absolute_file_url() {
    use core_foundation_sys::url::CFURLCreateWithFileSystemPathRelativeToBase;
    use std::path::PathBuf;

    let path = "/usr/local/foo";
    let file = "bar";

    let cfstr_path = CFString::from_static_string(path);
    let cfstr_file = CFString::from_static_string(file);
    let cfurl_base = CFURL::from_file_system_path(cfstr_path, kCFURLPOSIXPathStyle, true);
    let cfurl_relative: CFURL = unsafe {
        let url_ref = CFURLCreateWithFileSystemPathRelativeToBase(kCFAllocatorDefault,
            cfstr_file.as_concrete_TypeRef(),
            kCFURLPOSIXPathStyle,
            false as u8,
            cfurl_base.as_concrete_TypeRef());
        TCFType::wrap_under_create_rule(url_ref)
    };

    let mut absolute_path = PathBuf::from(path);
    absolute_path.push(file);

    assert_eq!(cfurl_relative.get_file_system_path(kCFURLPOSIXPathStyle).to_string(), file);
    assert_eq!(cfurl_relative.absolute().get_file_system_path(kCFURLPOSIXPathStyle).to_string(),
        absolute_path.to_str().unwrap());
}
