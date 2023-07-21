/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
extern crate minidump_writer;

use anyhow;
use libc::pid_t;
use minidump_writer::crash_context::CrashContext;
use minidump_writer::minidump_writer::MinidumpWriter;
use nsstring::nsCString;
use std::ffi::CStr;
use std::mem::{self, MaybeUninit};
use std::os::raw::c_char;

// This function will be exposed to C++
#[no_mangle]
pub unsafe extern "C" fn write_minidump_linux(
    dump_path: *const c_char,
    child: pid_t,
    child_blamed_thread: pid_t,
    error_msg: &mut nsCString,
) -> bool {
    assert!(!dump_path.is_null());
    let c_path = CStr::from_ptr(dump_path);
    let path = match c_path.to_str() {
        Ok(s) => s,
        Err(x) => {
            error_msg.assign(&format!(
                "Wrapper error. Path not convertable: {:#}",
                anyhow::Error::new(x)
            ));
            return false;
        }
    };

    let mut dump_file = match std::fs::OpenOptions::new()
        .create(true) // Create file if it doesn't exist
        .write(true) // Truncate file
        .open(path)
    {
        Ok(f) => f,
        Err(x) => {
            error_msg.assign(&format!(
                "Wrapper error when opening minidump destination at {:?}: {:#}",
                path,
                anyhow::Error::new(x)
            ));
            return false;
        }
    };

    match MinidumpWriter::new(child, child_blamed_thread).dump(&mut dump_file) {
        Ok(_) => {
            return true;
        }
        Err(x) => {
            error_msg.assign(&format!("{:#}", anyhow::Error::new(x)));
            return false;
        }
    }
}

#[allow(non_camel_case_types)]
#[cfg(not(target_arch = "arm"))]
type fpregset_t = crash_context::fpregset_t;
#[allow(non_camel_case_types)]
#[cfg(target_arch = "arm")]
pub struct fpregset_t {}

// This function will be exposed to C++
#[no_mangle]
pub unsafe extern "C" fn write_minidump_linux_with_context(
    dump_path: *const c_char,
    child: pid_t,
    ucontext: *const crash_context::ucontext_t,
    #[allow(unused)] float_state: *const fpregset_t,
    siginfo: *const libc::signalfd_siginfo,
    child_thread: libc::pid_t,
    error_msg: &mut nsCString,
) -> bool {
    let c_path = CStr::from_ptr(dump_path);

    let mut crash_context: MaybeUninit<crash_context::CrashContext> = mem::MaybeUninit::zeroed();
    let cc = &mut *crash_context.as_mut_ptr();

    core::ptr::copy_nonoverlapping(siginfo, &mut cc.siginfo, 1);
    core::ptr::copy_nonoverlapping(ucontext, &mut cc.context, 1);
    #[cfg(not(target_arch = "arm"))]
    core::ptr::copy_nonoverlapping(float_state, &mut cc.float_state, 1);

    cc.pid = child;
    cc.tid = child_thread;
    let crash_context = crash_context.assume_init();
    let crash_context = CrashContext {
        inner: crash_context,
    };

    let path = match c_path.to_str() {
        Ok(s) => s,
        Err(x) => {
            error_msg.assign(&format!(
                "Wrapper error. Path not convertable: {:#}",
                anyhow::Error::new(x)
            ));
            return false;
        }
    };

    let mut dump_file = match std::fs::OpenOptions::new()
        .create(true) // Create file if it doesn't exist
        .write(true) // Truncate file
        .open(path)
    {
        Ok(f) => f,
        Err(x) => {
            error_msg.assign(&format!(
                "Wrapper error when opening minidump destination at {:?}: {:#}",
                path,
                anyhow::Error::new(x)
            ));
            return false;
        }
    };

    match MinidumpWriter::new(child, child_thread)
        .set_crash_context(crash_context)
        .dump(&mut dump_file)
    {
        Ok(_) => {
            return true;
        }
        Err(x) => {
            error_msg.assign(&format!("{:#}", anyhow::Error::new(x)));
            return false;
        }
    }
}
