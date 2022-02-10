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
    pub fn duplicate(h: PlatformHandleType) -> Result<PlatformHandle, std::io::Error> {
        unsafe {
            let newfd = libc::dup(h);
            if !valid_handle(newfd) {
                return Err(std::io::Error::last_os_error());
            }
            Ok(PlatformHandle::from(newfd))
        }
    }

    #[cfg(windows)]
    pub fn duplicate(h: PlatformHandleType) -> Result<PlatformHandle, std::io::Error> {
        let dup = unsafe { duplicate_platform_handle(h, None) }?;
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
use winapi::shared::minwindef::DWORD;

// Duplicate `source_handle`.
// - If `target_pid` is `Some(...)`, `source_handle` is closed.
// - If `target_pid` is `None`, `source_handle` is not closed.
#[cfg(windows)]
pub(crate) unsafe fn duplicate_platform_handle(
    source_handle: PlatformHandleType,
    target_pid: Option<DWORD>,
) -> Result<PlatformHandleType, std::io::Error> {
    use winapi::shared::minwindef::FALSE;
    use winapi::um::{handleapi, processthreadsapi, winnt};

    let source = processthreadsapi::GetCurrentProcess();
    let (target, close_source) = if let Some(pid) = target_pid {
        let target = processthreadsapi::OpenProcess(winnt::PROCESS_DUP_HANDLE, FALSE, pid);
        if !valid_handle(target) {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "invalid target process",
            ));
        }
        (target, true)
    } else {
        (source, false)
    };

    let mut target_handle = std::ptr::null_mut();
    let mut options = winnt::DUPLICATE_SAME_ACCESS;
    if close_source {
        options |= winnt::DUPLICATE_CLOSE_SOURCE;
    }
    let ok = handleapi::DuplicateHandle(
        source,
        source_handle,
        target,
        &mut target_handle,
        0,
        FALSE,
        options,
    );
    handleapi::CloseHandle(target);
    if ok == FALSE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            "DuplicateHandle failed",
        ));
    }
    Ok(target_handle)
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
