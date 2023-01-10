//! Contains bindings for [`MiniDumpWriteDump`](https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump)
//! and related structures, as they are not present in `winapi` and we don't want
//! to depend on `windows-sys` due to version churn.
//!
//! Also has a binding for [`GetThreadContext`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext)
//! as the `CONTEXT` structures in `winapi` are not correctly aligned which can
//! cause crashes or bad data, so the [`crash_context::ffi::CONTEXT`] is used
//! instead. See [#63](https://github.com/rust-minidump/minidump-writer/issues/63)

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

pub use crash_context::ffi::{capture_context, CONTEXT, EXCEPTION_POINTERS, EXCEPTION_RECORD};
pub use winapi::{
    shared::minwindef::BOOL,
    um::{
        processthreadsapi::{
            GetCurrentProcess, GetCurrentProcessId, GetCurrentThread, GetCurrentThreadId,
            OpenProcess, OpenThread, ResumeThread, SuspendThread,
        },
        winnt::HANDLE,
    },
};

pub type MINIDUMP_TYPE = u32;

pub const MiniDumpNormal: MINIDUMP_TYPE = 0u32;

pub type MINIDUMP_CALLBACK_ROUTINE = Option<
    unsafe extern "system" fn(
        callbackparam: *mut ::core::ffi::c_void,
        callbackinput: *const MINIDUMP_CALLBACK_INPUT,
        callbackoutput: *mut MINIDUMP_CALLBACK_OUTPUT,
    ) -> BOOL,
>;

#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_CALLBACK_INPUT {
    dummy: u32,
}

#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_CALLBACK_OUTPUT {
    dummy: u32,
}
#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_CALLBACK_INFORMATION {
    pub CallbackRoutine: MINIDUMP_CALLBACK_ROUTINE,
    pub CallbackParam: *mut ::core::ffi::c_void,
}

#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_EXCEPTION_INFORMATION {
    pub ThreadId: u32,
    pub ExceptionPointers: *mut EXCEPTION_POINTERS,
    pub ClientPointers: BOOL,
}

#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_USER_STREAM {
    pub Type: u32,
    pub BufferSize: u32,
    pub Buffer: *mut ::core::ffi::c_void,
}

#[derive(Copy, Clone)]
#[repr(C, packed(4))]
pub struct MINIDUMP_USER_STREAM_INFORMATION {
    pub UserStreamCount: u32,
    pub UserStreamArray: *mut MINIDUMP_USER_STREAM,
}

extern "system" {
    pub fn GetThreadContext(hthread: HANDLE, lpcontext: *mut CONTEXT) -> BOOL;
    pub fn MiniDumpWriteDump(
        hprocess: HANDLE,
        processid: u32,
        hfile: HANDLE,
        dumptype: MINIDUMP_TYPE,
        exceptionparam: *const MINIDUMP_EXCEPTION_INFORMATION,
        userstreamparam: *const MINIDUMP_USER_STREAM_INFORMATION,
        callbackparam: *const MINIDUMP_CALLBACK_INFORMATION,
    ) -> BOOL;
}
