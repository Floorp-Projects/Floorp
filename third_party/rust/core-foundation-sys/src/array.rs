// Copyright 2013-2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use libc::c_void;

use base::{CFIndex, CFAllocatorRef, CFTypeID};

/// FIXME(pcwalton): This is wrong.
pub type CFArrayRetainCallBack = *const u8;

/// FIXME(pcwalton): This is wrong.
pub type CFArrayReleaseCallBack = *const u8;

/// FIXME(pcwalton): This is wrong.
pub type CFArrayCopyDescriptionCallBack = *const u8;

/// FIXME(pcwalton): This is wrong.
pub type CFArrayEqualCallBack = *const u8;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CFArrayCallBacks {
    pub version: CFIndex,
    pub retain: CFArrayRetainCallBack,
    pub release: CFArrayReleaseCallBack,
    pub copyDescription: CFArrayCopyDescriptionCallBack,
    pub equal: CFArrayEqualCallBack,
}

#[repr(C)]
pub struct __CFArray(c_void);

pub type CFArrayRef = *const __CFArray;

extern {
    /*
     * CFArray.h
     */
    pub static kCFTypeArrayCallBacks: CFArrayCallBacks;

    pub fn CFArrayCreate(allocator: CFAllocatorRef, values: *const *const c_void,
                     numValues: CFIndex, callBacks: *const CFArrayCallBacks) -> CFArrayRef;
    pub fn CFArrayCreateCopy(allocator: CFAllocatorRef , theArray: CFArrayRef) -> CFArrayRef;
    
    // CFArrayBSearchValues
    // CFArrayContainsValue
    pub fn CFArrayGetCount(theArray: CFArrayRef) -> CFIndex;
    // CFArrayGetCountOfValue
    // CFArrayGetFirstIndexOfValue
    // CFArrayGetLastIndexOfValue
    // CFArrayGetValues
    pub fn CFArrayGetValueAtIndex(theArray: CFArrayRef, idx: CFIndex) -> *const c_void;
    // CFArrayApplyFunction
    pub fn CFArrayGetTypeID() -> CFTypeID;
}
