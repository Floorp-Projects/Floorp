/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
extern crate minidump_writer_linux;

use libc::pid_t;
use std::ffi::CStr;
use std::os::raw::{c_char, c_void};

use minidump_writer_linux::crash_context::CrashContext;
use minidump_writer_linux::minidump_writer::MinidumpWriter;

// This function will be exposed to C++
#[no_mangle]
pub unsafe extern "C" fn write_minidump_linux(
    dump_path: *const c_char,
    child: pid_t,
    child_blamed_thread: pid_t,
) -> bool {
    assert!(!dump_path.is_null());
    let c_path = CStr::from_ptr(dump_path);

    let path = match c_path.to_str() {
        Ok(s) => s,
        Err(_) => {
            return false;
        }
    };

    let mut dump_file = match std::fs::OpenOptions::new()
        .create(true) // Create file if it doesn't exist
        .write(true) // Truncate file
        .open(path)
    {
        Ok(f) => f,
        Err(_) => {
            return false;
        }
    };

    MinidumpWriter::new(child, child_blamed_thread)
        .dump(&mut dump_file)
        .is_ok()
}

// This function will be exposed to C++
#[no_mangle]
pub unsafe extern "C" fn write_minidump_linux_with_context(
    dump_path: *const c_char,
    child: pid_t,
    context: *const c_void,
) -> bool {
    assert!(!dump_path.is_null());
    let c_path = CStr::from_ptr(dump_path);

    assert!(!context.is_null());
    let cc = (&*(context as *const CrashContext)).clone();

    let path = match c_path.to_str() {
        Ok(s) => s,
        Err(_) => {
            return false;
        }
    };

    let mut dump_file = match std::fs::OpenOptions::new()
        .create(true) // Create file if it doesn't exist
        .write(true) // Truncate file
        .open(path)
    {
        Ok(f) => f,
        Err(_) => {
            return false;
        }
    };

    MinidumpWriter::new(child, cc.tid)
        .set_crash_context(cc)
        .dump(&mut dump_file)
        .is_ok()
}
