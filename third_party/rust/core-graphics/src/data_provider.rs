// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};

use libc::{c_void, size_t};
use std::mem;
use std::ptr;

pub type CGDataProviderGetBytesCallback = *const u8;
pub type CGDataProviderReleaseInfoCallback = *const u8;
pub type CGDataProviderRewindCallback = *const u8;
pub type CGDataProviderSkipBytesCallback = *const u8;
pub type CGDataProviderSkipForwardCallback = *const u8;

pub type CGDataProviderGetBytePointerCallback = *const u8;
pub type CGDataProviderGetBytesAtOffsetCallback = *const u8;
pub type CGDataProviderReleaseBytePointerCallback = *const u8;
pub type CGDataProviderReleaseDataCallback = *const u8;
pub type CGDataProviderGetBytesAtPositionCallback = *const u8;

#[repr(C)]
pub struct __CGDataProvider;

pub type CGDataProviderRef = *const __CGDataProvider;

pub struct CGDataProvider {
    obj: CGDataProviderRef,
}

impl Drop for CGDataProvider {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl TCFType<CGDataProviderRef> for CGDataProvider {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CGDataProviderRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CGDataProviderRef) -> CGDataProvider {
        let reference: CGDataProviderRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CGDataProviderRef) -> CGDataProvider {
        CGDataProvider {
            obj: obj,
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CGDataProviderGetTypeID()
        }
    }
}

impl CGDataProvider {
    pub fn from_buffer(buffer: &[u8]) -> CGDataProvider {
        unsafe {
            let result = CGDataProviderCreateWithData(ptr::null_mut(),
                                                      buffer.as_ptr() as *const c_void,
                                                      buffer.len() as size_t,
                                                      ptr::null());
            TCFType::wrap_under_create_rule(result)
        }
    }
}

#[link(name = "ApplicationServices", kind = "framework")]
extern {
    //fn CGDataProviderCopyData
    //fn CGDataProviderCreateDirect
    //fn CGDataProviderCreateSequential
    //fn CGDataProviderCreateWithCFData
    fn CGDataProviderCreateWithData(info: *mut c_void,
                                    data: *const c_void,
                                    size: size_t,
                                    releaseData: CGDataProviderReleaseDataCallback
                                   ) -> CGDataProviderRef;
    //fn CGDataProviderCreateWithFilename(filename: *c_char) -> CGDataProviderRef;
    //fn CGDataProviderCreateWithURL
    fn CGDataProviderGetTypeID() -> CFTypeID;
    //fn CGDataProviderRelease(provider: CGDataProviderRef);
    //fn CGDataProviderRetain(provider: CGDataProviderRef) -> CGDataProviderRef;
}
