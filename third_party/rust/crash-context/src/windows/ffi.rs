//! Custom bindings to `CONTEXT` and some structures that relate to it, to both
//! deal with bugs in bindings, as well as not be dependent particularly on
//! `windows-sys` due the massive amount of version churn that crate introduces.
//!
//! Both [`winapi`](https://docs.rs/winapi/latest/winapi/) and
//! [`windows-sys`](https://docs.rs/windows-sys/latest/windows_sys/) have
//! incorrect bindings with regards to `CONTEXT` and its related structures.
//! These structures **must** be aligned to 16 bytes, but [are not](https://github.com/microsoft/win32metadata/issues/1044).

#![allow(non_snake_case)]

extern "C" {
    /// Reimplementation of [`RtlCaptureContext`](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext)
    ///
    /// As noted above, the structures from `winapi` and `windows-sys` have
    /// incorrect alignment.
    ///
    /// In addition, `RtlCaptureContext` only captures the state of the integer
    /// registers, while this implementation additionally captures floating point
    /// and vector state.
    ///
    /// The implementation is ported from Crashpad.
    ///
    /// - [x86](https://github.com/chromium/crashpad/blob/f742c1aa4aff1834dc55e97895f8cdfdfc945a21/util/misc/capture_context_win.asm)
    /// - [aarch64](https://github.com/chromium/crashpad/blob/f742c1aa4aff1834dc55e97895f8cdfdfc945a21/util/misc/capture_context_win_arm64.asm)
    pub fn capture_context(ctx: *mut CONTEXT);
}

pub use winapi::um::winnt::EXCEPTION_RECORD;

cfg_if::cfg_if! {
    if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use x86_64::*;
    } else if #[cfg(target_arch = "x86")] {
        compile_error!("Please file an issue if you care about this target");
        //mod x86;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use aarch64::*;
    }
}

#[derive(Copy, Clone)]
#[repr(C)]
pub struct EXCEPTION_POINTERS {
    pub ExceptionRecord: *mut EXCEPTION_RECORD,
    pub ContextRecord: *mut CONTEXT,
}
