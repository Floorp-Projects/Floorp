// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Core Foundation Bundle Type

pub use core_foundation_sys::bundle::*;
use core_foundation_sys::base::CFRelease;

use base::{TCFType};
use dictionary::CFDictionary;

/// A Bundle type.
pub struct CFBundle(CFBundleRef);

impl Drop for CFBundle {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl CFBundle {
    pub fn main_bundle() -> CFBundle {
        unsafe {
            let bundle_ref = CFBundleGetMainBundle();
            TCFType::wrap_under_get_rule(bundle_ref)
        }
    }

    pub fn info_dictionary(&self) -> CFDictionary {
        unsafe {
            let info_dictionary = CFBundleGetInfoDictionary(self.0);
            TCFType::wrap_under_get_rule(info_dictionary)
        }
    }
}

impl_TCFType!(CFBundle, CFBundleRef, CFBundleGetTypeID);

