// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_upper_case_globals)]

pub use core_foundation_sys::runloop::*;
use core_foundation_sys::base::{CFIndex, CFRelease};
use core_foundation_sys::base::{kCFAllocatorDefault, CFOptionFlags};
use core_foundation_sys::string::CFStringRef;
use core_foundation_sys::date::{CFAbsoluteTime, CFTimeInterval};

use base::{TCFType};
use string::{CFString};

pub struct CFRunLoop(CFRunLoopRef);

impl Drop for CFRunLoop {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFRunLoop, CFRunLoopRef, CFRunLoopGetTypeID);

impl CFRunLoop {
    pub fn get_current() -> CFRunLoop {
        unsafe {
            let run_loop_ref = CFRunLoopGetCurrent();
            TCFType::wrap_under_get_rule(run_loop_ref)
        }
    }

    pub fn get_main() -> CFRunLoop {
        unsafe {
            let run_loop_ref = CFRunLoopGetMain();
            TCFType::wrap_under_get_rule(run_loop_ref)
        }
    }

    pub fn run_current() {
        unsafe {
            CFRunLoopRun();
        }
    }

    pub fn stop(&self) {
        unsafe {
            CFRunLoopStop(self.0);
        }
    }

    pub fn current_mode(&self) -> Option<String> {
        unsafe {
            let string_ref = CFRunLoopCopyCurrentMode(self.0);
            if string_ref.is_null() {
                return None;
            }

            let cf_string: CFString = TCFType::wrap_under_create_rule(string_ref);
            Some(cf_string.to_string())
        }
    }

    pub fn contains_timer(&self, timer: &CFRunLoopTimer, mode: CFStringRef) -> bool {
        unsafe {
            CFRunLoopContainsTimer(self.0, timer.0, mode) != 0
        }
    }

    pub fn add_timer(&self, timer: &CFRunLoopTimer, mode: CFStringRef) {
        unsafe {
            CFRunLoopAddTimer(self.0, timer.0, mode);
        }
    }

}

pub struct CFRunLoopTimer(CFRunLoopTimerRef);

impl Drop for CFRunLoopTimer {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFRunLoopTimer, CFRunLoopTimerRef, CFRunLoopTimerGetTypeID);

impl CFRunLoopTimer {
    pub fn new(fireDate: CFAbsoluteTime, interval: CFTimeInterval, flags: CFOptionFlags, order: CFIndex, callout: CFRunLoopTimerCallBack, context: *mut CFRunLoopTimerContext) -> CFRunLoopTimer {
        unsafe {
            let timer_ref = CFRunLoopTimerCreate(kCFAllocatorDefault, fireDate, interval, flags, order, callout, context);
            TCFType::wrap_under_create_rule(timer_ref)
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use core_foundation_sys::date::{CFAbsoluteTime, CFAbsoluteTimeGetCurrent};
    use std::mem;
    use libc::c_void;

    #[test]
    fn wait_200_milliseconds() {
        let run_loop = CFRunLoop::get_current();
        let mut now = unsafe { CFAbsoluteTimeGetCurrent() };
        let mut context = unsafe { CFRunLoopTimerContext {
            version: 0,
            info: mem::transmute(&mut now),
            retain: mem::zeroed(),
            release: mem::zeroed(),
            copyDescription: mem::zeroed(),
        } };


        let run_loop_timer = CFRunLoopTimer::new(now + 0.20f64, 0f64, 0, 0, timer_popped, &mut context);
        run_loop.add_timer(&run_loop_timer, kCFRunLoopDefaultMode);

        CFRunLoop::run_current();
    }

    extern "C" fn timer_popped(_timer: CFRunLoopTimerRef, _info: *mut c_void) {
        let previous_now_ptr: *const CFAbsoluteTime = unsafe { mem::transmute(_info) };
        let previous_now = unsafe { *previous_now_ptr };
        let now = unsafe { CFAbsoluteTimeGetCurrent() };
        assert!(now - previous_now > 0.19 && now - previous_now < 0.21);
        CFRunLoop::get_current().stop();
    }
}
