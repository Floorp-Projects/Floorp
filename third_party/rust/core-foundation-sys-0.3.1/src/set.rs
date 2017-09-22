// Copyright 2013-2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use libc::c_void;

use base::{CFAllocatorRef, CFIndex, CFTypeID};

pub type CFSetRetainCallBack = *const u8;
pub type CFSetReleaseCallBack = *const u8;
pub type CFSetCopyDescriptionCallBack = *const u8;
pub type CFSetEqualCallBack = *const u8;
pub type CFSetHashCallBack = *const u8;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CFSetCallBacks {
    pub version: CFIndex,
    pub retain: CFSetRetainCallBack,
    pub release: CFSetReleaseCallBack,
    pub copyDescription: CFSetCopyDescriptionCallBack,
    pub equal: CFSetEqualCallBack,
    pub hash: CFSetHashCallBack,
}

#[repr(C)]
pub struct __CFSet(c_void);

pub type CFSetRef = *const __CFSet;

extern {
    /*
     * CFSet.h
     */

    pub static kCFTypeSetCallBacks: CFSetCallBacks;

    /* Creating Sets */
    pub fn CFSetCreate(allocator: CFAllocatorRef, values: *const *const c_void, numValues: CFIndex,
                       callBacks: *const CFSetCallBacks) -> CFSetRef;

    /* Applying a Function to Set Members */
    //fn CFSetApplyFunction

    pub fn CFSetGetTypeID() -> CFTypeID;
}

