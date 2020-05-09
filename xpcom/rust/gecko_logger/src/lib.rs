/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! This provides a way to direct rust logging into the gecko logger.

extern crate log;
use log::{Level, LevelFilter};
use std::boxed::Box;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::{Arc, RwLock};

extern "C" {
    fn ExternMozLog(tag: *const c_char, prio: c_int, text: *const c_char);
}

/// This is a static AtomicPtr that is normally null. When Gecko code
/// sets the log level for a module that uses a rust logger, we will
/// allocate this pointer and never change it.
/// This optimizes the fast path when there are no active logging modules.
/// That becomes only a null check. When the pointer is not null, we then
/// proceed to read the hashmap and see if the module has an appropriate
/// logging level.
static LOG_MODULE_MAP: AtomicPtr<Arc<RwLock<HashMap<&str, LevelFilter>>>> =
    AtomicPtr::new(ptr::null_mut());

/// This function takes a record to maybe log to Gecko.
/// It returns true if the record was handled by Gecko's logging, and false
/// otherwise.
pub fn log_to_gecko(record: &log::Record) -> bool {
    let module_map = LOG_MODULE_MAP.load(Ordering::Relaxed);

    // No logging modules have been set.
    if module_map.is_null() {
        return false;
    }

    let key = match record.module_path() {
        Some(key) => key,
        None => return false,
    };
    let level = {
        let arc = Arc::clone(unsafe { &*module_map });
        let map = arc.read().unwrap();
        match map.get(key) {
            None => return false, // This module is not being logged.
            Some(module_level) => module_level.clone(),
        }
    };

    if level < record.metadata().level() {
        return false;
    }

    // Map the log::Level to mozilla::LogLevel.
    let moz_log_level = match record.metadata().level() {
        Level::Error => 1, // Error
        Level::Warn => 2,  // Warning
        Level::Info => 3,  // Info
        Level::Debug => 4, // Debug
        Level::Trace => 5, // Verbose
    };

    let msg = CString::new(format!("{}", record.args())).unwrap();
    let tag = CString::new(key).unwrap();

    unsafe {
        ExternMozLog(tag.as_ptr(), moz_log_level, msg.as_ptr());
    }

    return true;
}

#[no_mangle]
pub extern "C" fn set_rust_log_level(module: *const c_char, level: u8) {
    // Convert the Gecko level to a rust LevelFilter.
    let rust_level = match level {
        1 => LevelFilter::Error,
        2 => LevelFilter::Warn,
        3 => LevelFilter::Info,
        4 => LevelFilter::Debug,
        5 => LevelFilter::Trace,
        _ => LevelFilter::Off,
    };

    // This is the name of the rust module that we're trying to log in Gecko.
    let mod_name = unsafe { CStr::from_ptr(module) }.to_str().unwrap();

    // When LOG_MODULE_MAP is null we will replace its value with a new map.
    // If the AtomicPtr has already been initialized, we just drop `new_map`
    // at the end of this function.
    let mut map = HashMap::new();
    // We insert to the map here, in case this is the first module and we can
    // just replace the map pointer and exit without acquiring the write lock.
    map.insert(mod_name, rust_level);
    let mut new_map = Box::new(Arc::new(RwLock::new(map)));

    // We only use the map when the first module is actually logging
    let myptr = if rust_level == LevelFilter::Off {
        ptr::null_mut()
    } else {
        new_map.as_mut()
    };

    // This will compare and swap the pointer. If the pointer is null, we will
    // replace it with a pointer to `new_map`. After this the value of the
    // pointer will never change. When we do replace the null pointer, we must
    // then pass the ownership of `new_map` to LOG_MODULE_MAP.
    let old_ptr = LOG_MODULE_MAP.compare_and_swap(ptr::null_mut(), myptr, Ordering::SeqCst);
    if old_ptr.is_null() {
        if !myptr.is_null() {
            log::set_max_level(rust_level);

            // Consume new_map so it doesn't get freed. LOG_MODULE_MAP is now
            // the owner of that memory.
            let _ = Box::into_raw(new_map);
        }
        return;
    }

    // Insert the module into the hashmap.
    let arc = Arc::clone(unsafe { &*old_ptr });
    let mut map = arc.write().unwrap();
    map.insert(mod_name, rust_level);

    // Figure out the max level of all the modules.
    let max = map.values().max().unwrap_or(&LevelFilter::Off);
    log::set_max_level(*max);
}
