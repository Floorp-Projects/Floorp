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

use base::{TCFType};
use date::{CFAbsoluteTime, CFTimeInterval};
use string::{CFString};

pub type CFRunLoopMode = CFStringRef;

pub struct CFRunLoop(CFRunLoopRef);

impl Drop for CFRunLoop {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFRunLoop, CFRunLoopRef, CFRunLoopGetTypeID);
impl_CFTypeDescription!(CFRunLoop);

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

    pub fn contains_timer(&self, timer: &CFRunLoopTimer, mode: CFRunLoopMode) -> bool {
        unsafe {
            CFRunLoopContainsTimer(self.0, timer.0, mode) != 0
        }
    }

    pub fn add_timer(&self, timer: &CFRunLoopTimer, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopAddTimer(self.0, timer.0, mode);
        }
    }

    pub fn remove_timer(&self, timer: &CFRunLoopTimer, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopRemoveTimer(self.0, timer.0, mode);
        }
    }

    pub fn contains_source(&self, source: &CFRunLoopSource, mode: CFRunLoopMode) -> bool {
        unsafe {
            CFRunLoopContainsSource(self.0, source.0, mode) != 0
        }
    }

    pub fn add_source(&self, source: &CFRunLoopSource, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopAddSource(self.0, source.0, mode);
        }
    }

    pub fn remove_source(&self, source: &CFRunLoopSource, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopRemoveSource(self.0, source.0, mode);
        }
    }

    pub fn contains_observer(&self, observer: &CFRunLoopObserver, mode: CFRunLoopMode) -> bool {
        unsafe {
            CFRunLoopContainsObserver(self.0, observer.0, mode) != 0
        }
    }

    pub fn add_observer(&self, observer: &CFRunLoopObserver, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopAddObserver(self.0, observer.0, mode);
        }
    }

    pub fn remove_observer(&self, observer: &CFRunLoopObserver, mode: CFRunLoopMode) {
        unsafe {
            CFRunLoopRemoveObserver(self.0, observer.0, mode);
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


pub struct CFRunLoopSource(CFRunLoopSourceRef);

impl Drop for CFRunLoopSource {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFRunLoopSource, CFRunLoopSourceRef, CFRunLoopSourceGetTypeID);


pub struct CFRunLoopObserver(CFRunLoopObserverRef);

impl Drop for CFRunLoopObserver {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFRunLoopObserver, CFRunLoopObserverRef, CFRunLoopObserverGetTypeID);

#[cfg(test)]
mod test {
    use super::*;
    use date::{CFDate, CFAbsoluteTime};
    use std::mem;
    use libc::c_void;

    #[test]
    fn wait_200_milliseconds() {
        let run_loop = CFRunLoop::get_current();
        let mut now = CFDate::now().abs_time();
        let mut context = unsafe { CFRunLoopTimerContext {
            version: 0,
            info: mem::transmute(&mut now),
            retain: mem::zeroed(),
            release: mem::zeroed(),
            copyDescription: mem::zeroed(),
        } };


        let run_loop_timer = CFRunLoopTimer::new(now + 0.20f64, 0f64, 0, 0, timer_popped, &mut context);
        unsafe {
            run_loop.add_timer(&run_loop_timer, kCFRunLoopDefaultMode);
        }
        CFRunLoop::run_current();
    }

    extern "C" fn timer_popped(_timer: CFRunLoopTimerRef, info: *mut c_void) {
        let previous_now_ptr: *const CFAbsoluteTime = unsafe { mem::transmute(info) };
        let previous_now = unsafe { *previous_now_ptr };
        let now = CFDate::now().abs_time();
        assert!(now - previous_now > 0.19 && now - previous_now < 0.21);
        CFRunLoop::get_current().stop();
    }
}
