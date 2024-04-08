/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! FfiDefault trait
//!
//! When we make a FFI call into Rust we always need to return a value, even if that value will be
//! ignored because we're flagging an exception.  This trait defines what that value is for our
//! supported FFI types.

use paste::paste;

pub trait FfiDefault {
    fn ffi_default() -> Self;
}

// Most types can be handled by delegating to Default
macro_rules! impl_ffi_default_with_default {
    ($($T:ty,)+) => { impl_ffi_default_with_default!($($T),+); };
    ($($T:ty),*) => {
            $(
                paste! {
                    impl FfiDefault for $T {
                        fn ffi_default() -> Self {
                            $T::default()
                        }
                    }
                }
            )*
    };
}

impl_ffi_default_with_default! {
    bool, i8, u8, i16, u16, i32, u32, i64, u64, f32, f64
}

// Implement FfiDefault for the remaining types
impl FfiDefault for () {
    fn ffi_default() {}
}

impl FfiDefault for crate::Handle {
    fn ffi_default() -> Self {
        Self::default()
    }
}

impl FfiDefault for *const std::ffi::c_void {
    fn ffi_default() -> Self {
        std::ptr::null()
    }
}

impl FfiDefault for crate::RustBuffer {
    fn ffi_default() -> Self {
        unsafe { Self::from_raw_parts(std::ptr::null_mut(), 0, 0) }
    }
}

impl FfiDefault for crate::ForeignFuture {
    fn ffi_default() -> Self {
        extern "C" fn free(_handle: u64) {}
        crate::ForeignFuture { handle: 0, free }
    }
}

impl<T> FfiDefault for Option<T> {
    fn ffi_default() -> Self {
        None
    }
}
