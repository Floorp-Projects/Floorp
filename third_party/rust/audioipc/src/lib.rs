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
#[macro_use]
extern crate serde_derive;
#[macro_use]
extern crate futures;
#[macro_use]
extern crate tokio_io;

mod async_msg;
#[cfg(unix)]
mod cmsg;
pub mod codec;
pub mod core;
#[allow(deprecated)]
pub mod errors;
#[cfg(unix)]
pub mod fd_passing;
#[cfg(unix)]
pub use crate::fd_passing as platformhandle_passing;
#[cfg(windows)]
pub mod handle_passing;
#[cfg(windows)]
pub use handle_passing as platformhandle_passing;
pub mod frame;
pub mod messages;
#[cfg(unix)]
mod msg;
pub mod rpc;
pub mod shm;

// TODO: Remove local fork when https://github.com/tokio-rs/tokio/pull/1294 is resolved.
#[cfg(unix)]
mod tokio_uds_stream;

#[cfg(windows)]
mod tokio_named_pipes;

pub use crate::messages::{ClientMessage, ServerMessage};

// TODO: Remove hardcoded size and allow allocation based on cubeb backend requirements.
pub const SHM_AREA_SIZE: usize = 2 * 1024 * 1024;

#[cfg(unix)]
use std::os::unix::io::IntoRawFd;
#[cfg(windows)]
use std::os::windows::io::IntoRawHandle;

use std::cell::RefCell;

// This must match the definition of
// ipc::FileDescriptor::PlatformHandleType in Gecko.
#[cfg(windows)]
pub type PlatformHandleType = std::os::windows::raw::HANDLE;
#[cfg(unix)]
pub type PlatformHandleType = libc::c_int;

// This stands in for RawFd/RawHandle.
#[derive(Clone, Debug)]
pub struct PlatformHandle(RefCell<Inner>);

#[derive(Clone, Debug)]
struct Inner {
    handle: PlatformHandleType,
    owned: bool,
}

unsafe impl Send for PlatformHandle {}

pub const INVALID_HANDLE_VALUE: PlatformHandleType = -1isize as PlatformHandleType;

// Custom serialization to treat HANDLEs as i64.  This is not valid in
// general, but after sending the HANDLE value to a remote process we
// use it to create a valid HANDLE via DuplicateHandle.
// To avoid duplicating the serialization code, we're lazy and treat
// file descriptors as i64 rather than i32.
impl serde::Serialize for PlatformHandle {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let h = self.0.borrow();
        serializer.serialize_i64(h.handle as i64)
    }
}

struct PlatformHandleVisitor;
impl<'de> serde::de::Visitor<'de> for PlatformHandleVisitor {
    type Value = PlatformHandle;

    fn expecting(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str("an integer between -2^63 and 2^63")
    }

    fn visit_i64<E>(self, value: i64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let owned = cfg!(windows);
        Ok(PlatformHandle::new(value as PlatformHandleType, owned))
    }
}

impl<'de> serde::Deserialize<'de> for PlatformHandle {
    fn deserialize<D>(deserializer: D) -> Result<PlatformHandle, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_i64(PlatformHandleVisitor)
    }
}

#[cfg(unix)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    handle >= 0
}

#[cfg(windows)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    const NULL_HANDLE_VALUE: PlatformHandleType = 0isize as PlatformHandleType;
    handle != INVALID_HANDLE_VALUE && handle != NULL_HANDLE_VALUE
}

impl PlatformHandle {
    pub fn new(raw: PlatformHandleType, owned: bool) -> PlatformHandle {
        assert!(valid_handle(raw));
        let inner = Inner { handle: raw, owned };
        PlatformHandle(RefCell::new(inner))
    }

    #[cfg(windows)]
    pub fn from<T: IntoRawHandle>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_handle(), true)
    }

    #[cfg(unix)]
    pub fn from<T: IntoRawFd>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_fd(), true)
    }

    #[allow(clippy::missing_safety_doc)]
    #[allow(clippy::wrong_self_convention)]
    pub unsafe fn into_raw(&self) -> PlatformHandleType {
        let mut h = self.0.borrow_mut();
        assert!(h.owned);
        h.owned = false;
        h.handle
    }

    #[allow(clippy::missing_safety_doc)]
    pub unsafe fn as_raw(&self) -> PlatformHandleType {
        self.0.borrow().handle
    }

    #[cfg(windows)]
    pub fn duplicate(h: PlatformHandleType) -> Result<PlatformHandle, std::io::Error> {
        let dup = unsafe { platformhandle_passing::duplicate_platformhandle(h, None, false) }?;
        Ok(PlatformHandle::new(dup, true))
    }
}

impl Drop for PlatformHandle {
    fn drop(&mut self) {
        let inner = self.0.borrow();
        if inner.owned {
            unsafe { close_platformhandle(inner.handle) }
        }
    }
}

#[cfg(unix)]
unsafe fn close_platformhandle(handle: PlatformHandleType) {
    libc::close(handle);
}

#[cfg(windows)]
unsafe fn close_platformhandle(handle: PlatformHandleType) {
    winapi::um::handleapi::CloseHandle(handle);
}

#[cfg(unix)]
pub mod messagestream_unix;
#[cfg(unix)]
pub use crate::messagestream_unix::*;

#[cfg(windows)]
pub mod messagestream_win;
#[cfg(windows)]
pub use messagestream_win::*;

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
