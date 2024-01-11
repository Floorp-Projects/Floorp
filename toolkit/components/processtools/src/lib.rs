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

#[cfg(target_os = "windows")]
struct Handle(winapi::um::winnt::HANDLE);

#[cfg(target_os = "windows")]
impl Handle {
    fn from_raw(raw: winapi::um::winnt::HANDLE) -> Option<Self> {
        (raw != std::ptr::null_mut() && raw != winapi::um::handleapi::INVALID_HANDLE_VALUE)
            .then_some(Handle(raw))
    }

    fn raw(self: &Self) -> winapi::um::winnt::HANDLE {
        self.0
    }
}

#[cfg(target_os = "windows")]
impl Drop for Handle {
    fn drop(&mut self) {
        unsafe {
            winapi::um::handleapi::CloseHandle(self.raw());
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn new_process_tools_service(result: *mut *const nsIProcessToolsService) {
    let service: RefPtr<ProcessToolsService> = ProcessToolsService::new();
    RefPtr::new(service.coerce::<nsIProcessToolsService>()).forget(&mut *result);
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
        let handle = Handle::from_raw(handle).ok_or(NS_ERROR_NOT_AVAILABLE)?;

        let result = unsafe {
            winapi::um::processthreadsapi::TerminateProcess(
                /* hProcess */ handle.raw(),
                /* uExitCode */ 0,
            )
        };

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
        let ntdll = unsafe {
            winapi::um::libloaderapi::GetModuleHandleA(
                /* lpModuleName */ std::mem::transmute(b"ntdll.dll\0".as_ptr()),
            )
        };
        if ntdll.is_null() {
            return Err(NS_ERROR_NOT_AVAILABLE);
        }

        let dbg_break_point = unsafe {
            winapi::um::libloaderapi::GetProcAddress(
                /* hModule */ ntdll,
                /* lpProcName */ std::mem::transmute(b"DbgBreakPoint\0".as_ptr()),
            )
        };
        if dbg_break_point.is_null() {
            return Err(NS_ERROR_NOT_AVAILABLE);
        }

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
        let target_proc = Handle::from_raw(target_proc).ok_or(NS_ERROR_NOT_AVAILABLE)?;

        let new_thread = unsafe {
            winapi::um::processthreadsapi::CreateRemoteThread(
                /* hProcess */ target_proc.raw(),
                /* lpThreadAttributes */ std::ptr::null_mut(),
                /* dwStackSize */ 0,
                /* lpStartAddress */ Some(std::mem::transmute(dbg_break_point)),
                /* lpParameter */ std::ptr::null_mut(),
                /* dwCreationFlags */ 0,
                /* lpThreadId */ std::ptr::null_mut(),
            )
        };
        let new_thread = Handle::from_raw(new_thread).ok_or(NS_ERROR_FAILURE)?;

        unsafe {
            winapi::um::synchapi::WaitForSingleObject(
                new_thread.raw(),
                winapi::um::winbase::INFINITE,
            );
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
