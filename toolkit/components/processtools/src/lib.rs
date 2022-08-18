/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(not(target_os = "windows"))]
extern crate libc;
#[cfg(target_os = "windows")]
extern crate winapi;

extern crate nserror;
extern crate xpcom;

use std::convert::TryInto;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use xpcom::{interfaces::nsIProcessToolsService, xpcom, xpcom_method, RefPtr};

// Separate this `use` to avoid build-breaking warnings.
#[cfg(target_os = "windows")]
use nserror::NS_ERROR_NOT_AVAILABLE;

#[no_mangle]
pub unsafe extern "C" fn new_process_tools_service(result: *mut *const nsIProcessToolsService) {
    let service: RefPtr<ProcessToolsService> = ProcessToolsService::new();
    RefPtr::new(service.coerce::<nsIProcessToolsService>()).forget(&mut *result);
}

#[cfg(target_os = "windows")]
use std::ffi::c_void;

// We want to generate an exception that can be caught by the exception handler
// when injecting in a remote process, STATUS_ACCESS_VIOLATION seems not to do,
// it but the following code generates a STATUS_ILLEGAL_INSTRUCTION that seems
// to do the trick
#[cfg(target_os = "windows")]
pub unsafe extern "system" fn crash_illegal_instruction(_arg: *mut c_void) -> u32 {
    std::ptr::null_mut::<u32>().write(1);
    0
}

// Implementation note:
//
// We're following the strategy employed by the `kvstore`.
// See https://searchfox.org/mozilla-central/rev/a87a1c3b543475276e6d57a7a80cb02f3e42b6ed/toolkit/components/kvstore/src/lib.rs#78

#[xpcom(implement(nsIProcessToolsService), atomic)]
pub struct ProcessToolsService {}

impl ProcessToolsService {
    pub fn new() -> RefPtr<ProcessToolsService> {
        ProcessToolsService::allocate(InitProcessToolsService {})
    }

    // Method `kill`.

    xpcom_method!(
        kill => Kill(id: u64)
    );

    #[cfg(target_os = "windows")]
    pub fn kill(&self, pid: u64) -> Result<(), nsresult> {
        let handle = unsafe {
            winapi::um::processthreadsapi::OpenProcess(
                /* dwDesiredAccess */
                winapi::um::winnt::PROCESS_TERMINATE | winapi::um::winnt::SYNCHRONIZE,
                /* bInheritHandle */ 0,
                /* dwProcessId */ pid.try_into().unwrap(),
            )
        };
        if handle.is_null() {
            // Could not open process.
            return Err(NS_ERROR_NOT_AVAILABLE);
        }

        let result = unsafe {
            winapi::um::processthreadsapi::TerminateProcess(
                /* hProcess */ handle, /* uExitCode */ 0,
            )
        };

        // Close handle regardless of success.
        let _ = unsafe { winapi::um::handleapi::CloseHandle(handle) };

        if result == 0 {
            return Err(NS_ERROR_FAILURE);
        }
        Ok(())
    }

    #[cfg(not(target_os = "windows"))]
    pub fn kill(&self, pid: u64) -> Result<(), nsresult> {
        let pid = pid.try_into().or(Err(NS_ERROR_FAILURE))?;
        let result = unsafe { libc::kill(pid, libc::SIGKILL) };
        if result == 0 {
            Ok(())
        } else {
            Err(NS_ERROR_FAILURE)
        }
    }

    // Method `crash`

    xpcom_method!(
        crash => Crash(id: u64)
    );

    #[cfg(target_os = "windows")]
    pub fn crash(&self, pid: u64) -> Result<(), nsresult> {
        let target_proc = unsafe {
            winapi::um::processthreadsapi::OpenProcess(
                /* dwDesiredAccess */
                winapi::um::winnt::PROCESS_VM_OPERATION
                    | winapi::um::winnt::PROCESS_CREATE_THREAD
                    | winapi::um::winnt::PROCESS_QUERY_INFORMATION,
                /* bInheritHandle */ 0,
                /* dwProcessId */ pid.try_into().unwrap(),
            )
        };
        if target_proc.is_null() {
            // Could not open process.
            return Err(NS_ERROR_NOT_AVAILABLE);
        }

        let new_thread = unsafe {
            winapi::um::processthreadsapi::CreateRemoteThread(
                /* hProcess */ target_proc,
                /* lpThreadAttributes */ std::ptr::null_mut(),
                /* dwStackSize */ 0,
                /* lpStartAddress */ Some(crash_illegal_instruction),
                /* lpParameter */ std::ptr::null_mut(),
                /* dwCreationFlags */ 0,
                /* lpThreadId */ std::ptr::null_mut(),
            )
        };

        // Close handle regardless of success.
        let _ = unsafe {
            winapi::um::synchapi::WaitForSingleObject(new_thread, winapi::um::winbase::INFINITE);
            winapi::um::handleapi::CloseHandle(new_thread);
            winapi::um::handleapi::CloseHandle(target_proc);
        };

        if new_thread.is_null() {
            return Err(NS_ERROR_FAILURE);
        }
        Ok(())
    }

    #[cfg(not(target_os = "windows"))]
    pub fn crash(&self, pid: u64) -> Result<(), nsresult> {
        let pid = pid.try_into().or(Err(NS_ERROR_FAILURE))?;
        let result = unsafe { libc::kill(pid, libc::SIGABRT) };
        if result == 0 {
            Ok(())
        } else {
            Err(NS_ERROR_FAILURE)
        }
    }

    // Attribute `pid`

    xpcom_method!(
        get_pid => GetPid() -> u64
    );

    #[cfg(not(target_os = "windows"))]
    pub fn get_pid(&self) -> Result<u64, nsresult> {
        let pid = unsafe { libc::getpid() } as u64;
        Ok(pid)
    }

    #[cfg(target_os = "windows")]
    pub fn get_pid(&self) -> Result<u64, nsresult> {
        let pid = unsafe { winapi::um::processthreadsapi::GetCurrentProcessId() } as u64;
        Ok(pid)
    }
}
