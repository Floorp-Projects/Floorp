/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

///! Profiler API for thread registration and unregistration.
use crate::gecko_bindings::bindings;
use std::ffi::CString;

/// Register a thread with the Gecko Profiler.
pub fn register_thread(thread_name: &str) {
    let name = CString::new(thread_name).unwrap();
    unsafe {
        // gecko_profiler_register_thread copies the passed name here.
        bindings::gecko_profiler_register_thread(name.as_ptr());
    }
}

/// Unregister a thread with the Gecko Profiler.
pub fn unregister_thread() {
    unsafe {
        bindings::gecko_profiler_unregister_thread();
    }
}
