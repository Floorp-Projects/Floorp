// Copyright 2013-2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use libc::{c_void};

use base::{CFAllocatorRef, CFIndex, CFTypeID, Boolean};

pub type CFDictionaryApplierFunction = extern "C" fn (key: *const c_void,
                                                      value: *const c_void,
                                                      context: *mut c_void);
pub type CFDictionaryCopyDescriptionCallBack = *const u8;
pub type CFDictionaryEqualCallBack = *const u8;
pub type CFDictionaryHashCallBack = *const u8;
pub type CFDictionaryReleaseCallBack = *const u8;
pub type CFDictionaryRetainCallBack = *const u8;

#[allow(dead_code)]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CFDictionaryKeyCallBacks {
    pub version: CFIndex,
    pub retain: CFDictionaryRetainCallBack,
    pub release: CFDictionaryReleaseCallBack,
    pub copyDescription: CFDictionaryCopyDescriptionCallBack,
    pub equal: CFDictionaryEqualCallBack,
    pub hash: CFDictionaryHashCallBack
}

#[allow(dead_code)]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CFDictionaryValueCallBacks {
    pub version: CFIndex,
    pub retain: CFDictionaryRetainCallBack,
    pub release: CFDictionaryReleaseCallBack,
    pub copyDescription: CFDictionaryCopyDescriptionCallBack,
    pub equal: CFDictionaryEqualCallBack
}

#[repr(C)]
pub struct __CFDictionary(c_void);

pub type CFDictionaryRef = *const __CFDictionary;
pub type CFMutableDictionaryRef = *const __CFDictionary;

extern {
    /*
     * CFDictionary.h
     */

    pub static kCFTypeDictionaryKeyCallBacks: CFDictionaryKeyCallBacks;
    pub static kCFTypeDictionaryValueCallBacks: CFDictionaryValueCallBacks;

    pub fn CFDictionaryContainsKey(theDict: CFDictionaryRef, key: *const c_void) -> Boolean;
    pub fn CFDictionaryCreate(allocator: CFAllocatorRef, keys: *const *const c_void, values: *const *const c_void,
                              numValues: CFIndex, keyCallBacks: *const CFDictionaryKeyCallBacks,
                              valueCallBacks: *const CFDictionaryValueCallBacks)
                              -> CFDictionaryRef;
    pub fn CFDictionaryGetCount(theDict: CFDictionaryRef) -> CFIndex;
    pub fn CFDictionaryGetTypeID() -> CFTypeID;
    pub fn CFDictionaryGetValueIfPresent(theDict: CFDictionaryRef, key: *const c_void, value: *mut *const c_void)
                                         -> Boolean;
    pub fn CFDictionaryApplyFunction(theDict: CFDictionaryRef,
                                     applier: CFDictionaryApplierFunction,
                                     context: *mut c_void);
    pub fn CFDictionarySetValue(theDict: CFMutableDictionaryRef,
                                key: *const c_void,
                                value: *const c_void);
    pub fn CFDictionaryGetKeysAndValues(theDict: CFDictionaryRef,
                                        keys: *mut *const c_void,
                                        values: *mut *const c_void);

}
