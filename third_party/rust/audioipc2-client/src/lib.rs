// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![warn(unused_extern_crates)]

#[macro_use]
extern crate log;

#[macro_use]
mod send_recv;
mod context;
mod stream;

use crate::context::ClientContext;
use crate::stream::ClientStream;
use audioipc::PlatformHandleType;
use cubeb_backend::{capi, ffi};
use std::os::raw::{c_char, c_int};

thread_local!(static IN_CALLBACK: std::cell::RefCell<bool> = const { std::cell::RefCell::new(false) });
thread_local!(static AUDIOIPC_INIT_PARAMS: std::cell::RefCell<Option<AudioIpcInitParams>> = const { std::cell::RefCell::new(None) });

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AudioIpcInitParams {
    // Fields only need to be public for ipctest.
    pub server_connection: PlatformHandleType,
    pub pool_size: usize,
    pub stack_size: usize,
    pub thread_create_callback: Option<extern "C" fn(*const ::std::os::raw::c_char)>,
    pub thread_destroy_callback: Option<extern "C" fn()>,
}

unsafe impl Send for AudioIpcInitParams {}

fn set_in_callback(in_callback: bool) {
    IN_CALLBACK.with(|b| {
        assert_eq!(*b.borrow(), !in_callback);
        *b.borrow_mut() = in_callback;
    });
}

fn run_in_callback<F, R>(f: F) -> R
where
    F: FnOnce() -> R,
{
    set_in_callback(true);

    let r = f();

    set_in_callback(false);

    r
}

fn assert_not_in_callback() {
    IN_CALLBACK.with(|b| {
        assert!(!*b.borrow());
    });
}

#[allow(clippy::missing_safety_doc)]
#[no_mangle]
/// Entry point from C code.
pub unsafe extern "C" fn audioipc2_client_init(
    c: *mut *mut ffi::cubeb,
    context_name: *const c_char,
    init_params: *const AudioIpcInitParams,
) -> c_int {
    if init_params.is_null() {
        return cubeb_backend::ffi::CUBEB_ERROR;
    }

    let init_params = &*init_params;

    AUDIOIPC_INIT_PARAMS.with(|p| {
        *p.borrow_mut() = Some(*init_params);
    });
    capi::capi_init::<ClientContext>(c, context_name)
}
