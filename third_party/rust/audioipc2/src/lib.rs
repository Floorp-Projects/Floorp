// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#![warn(unused_extern_crates)]
#![recursion_limit = "1024"]
#[macro_use]
extern crate error_chain;
#[macro_use]
extern crate log;

pub mod codec;
#[allow(deprecated)]
pub mod errors;
pub mod messages;
pub mod shm;

pub mod ipccore;
pub mod rpccore;
pub mod sys;

pub use crate::messages::{ClientMessage, ServerMessage};

#[cfg(unix)]
use std::os::unix::io::IntoRawFd;
#[cfg(windows)]
use std::os::windows::io::IntoRawHandle;

use std::io::Result;

// This must match the definition of
// ipc::FileDescriptor::PlatformHandleType in Gecko.
#[cfg(windows)]
pub type PlatformHandleType = std::os::windows::raw::HANDLE;
#[cfg(unix)]
pub type PlatformHandleType = libc::c_int;

// This stands in for RawFd/RawHandle.
#[derive(Debug)]
pub struct PlatformHandle(PlatformHandleType);

#[cfg(unix)]
pub const INVALID_HANDLE_VALUE: PlatformHandleType = -1isize as PlatformHandleType;

#[cfg(windows)]
pub const INVALID_HANDLE_VALUE: PlatformHandleType = winapi::um::handleapi::INVALID_HANDLE_VALUE;

#[cfg(unix)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    handle >= 0
}

#[cfg(windows)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    handle != INVALID_HANDLE_VALUE && !handle.is_null()
}

impl PlatformHandle {
    pub fn new(raw: PlatformHandleType) -> PlatformHandle {
        assert!(valid_handle(raw));
        PlatformHandle(raw)
    }

    #[cfg(windows)]
    pub fn from<T: IntoRawHandle>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_handle())
    }

    #[cfg(unix)]
    pub fn from<T: IntoRawFd>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_fd())
    }

    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn into_raw(self) -> PlatformHandleType {
        let handle = self.0;
        std::mem::forget(self);
        handle
    }

    #[cfg(unix)]
    pub fn duplicate(h: PlatformHandleType) -> Result<PlatformHandle> {
        unsafe {
            let newfd = libc::dup(h);
            if !valid_handle(newfd) {
                return Err(std::io::Error::last_os_error());
            }
            Ok(PlatformHandle::from(newfd))
        }
    }

    #[allow(clippy::missing_safety_doc)]
    #[cfg(windows)]
    pub unsafe fn duplicate(h: PlatformHandleType) -> Result<PlatformHandle> {
        let dup = duplicate_platform_handle(h, None)?;
        Ok(PlatformHandle::new(dup))
    }
}

impl Drop for PlatformHandle {
    fn drop(&mut self) {
        unsafe { close_platform_handle(self.0) }
    }
}

#[cfg(unix)]
unsafe fn close_platform_handle(handle: PlatformHandleType) {
    libc::close(handle);
}

#[cfg(windows)]
unsafe fn close_platform_handle(handle: PlatformHandleType) {
    winapi::um::handleapi::CloseHandle(handle);
}

#[cfg(windows)]
use winapi::shared::minwindef::{DWORD, FALSE};
#[cfg(windows)]
use winapi::um::{handleapi, processthreadsapi, winnt};

// Duplicate `source_handle` to `target_pid`.  Returns the value of the new handle inside the target process.
// If `target_pid` is `None`, `source_handle` is duplicated in the current process.
#[cfg(windows)]
pub(crate) unsafe fn duplicate_platform_handle(
    source_handle: PlatformHandleType,
    target_pid: Option<DWORD>,
) -> Result<PlatformHandleType> {
    let source_process = processthreadsapi::GetCurrentProcess();
    let target_process = if let Some(pid) = target_pid {
        let target = processthreadsapi::OpenProcess(winnt::PROCESS_DUP_HANDLE, FALSE, pid);
        if !valid_handle(target) {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "invalid target process",
            ));
        }
        Some(target)
    } else {
        None
    };

    let mut target_handle = std::ptr::null_mut();
    let ok = handleapi::DuplicateHandle(
        source_process,
        source_handle,
        target_process.unwrap_or(source_process),
        &mut target_handle,
        0,
        FALSE,
        winnt::DUPLICATE_SAME_ACCESS,
    );
    if let Some(target) = target_process {
        handleapi::CloseHandle(target);
    };
    if ok == FALSE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            "DuplicateHandle failed",
        ));
    }
    Ok(target_handle)
}

// Close `target_handle_to_close` inside target process `target_pid` using a
// special invocation of `DuplicateHandle`. See
// https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle#:~:text=Normally%20the%20target,dwOptions%20to%20DUPLICATE_CLOSE_SOURCE.
#[cfg(windows)]
pub(crate) unsafe fn close_target_handle(
    target_handle_to_close: PlatformHandleType,
    target_pid: DWORD,
) -> Result<()> {
    let target_process =
        processthreadsapi::OpenProcess(winnt::PROCESS_DUP_HANDLE, FALSE, target_pid);
    if !valid_handle(target_process) {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            "invalid target process",
        ));
    }

    let ok = handleapi::DuplicateHandle(
        target_process,
        target_handle_to_close,
        std::ptr::null_mut(),
        std::ptr::null_mut(),
        0,
        FALSE,
        winnt::DUPLICATE_CLOSE_SOURCE,
    );
    handleapi::CloseHandle(target_process);
    if ok == FALSE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            "DuplicateHandle failed",
        ));
    }
    Ok(())
}

#[cfg(windows)]
pub fn server_platform_init() {
    use winapi::shared::winerror;
    use winapi::um::combaseapi;
    use winapi::um::objbase;

    unsafe {
        let r = combaseapi::CoInitializeEx(std::ptr::null_mut(), objbase::COINIT_MULTITHREADED);
        assert!(winerror::SUCCEEDED(r));
    }
}

#[cfg(unix)]
pub fn server_platform_init() {}
