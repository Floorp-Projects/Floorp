// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#![allow(non_camel_case_types)]

use ffi;
use std::os::raw::{c_char, c_float, c_int, c_uint, c_void};

#[repr(C)]
pub struct Ops {
    pub init: Option<
        unsafe extern "C" fn(context: *mut *mut ffi::cubeb, context_name: *const c_char) -> c_int,
    >,
    pub get_backend_id: Option<unsafe extern "C" fn(context: *mut ffi::cubeb) -> *const c_char>,
    pub get_max_channel_count:
        Option<unsafe extern "C" fn(context: *mut ffi::cubeb, max_channels: *mut c_uint) -> c_int>,
    pub get_min_latency: Option<
        unsafe extern "C" fn(
            context: *mut ffi::cubeb,
            params: ffi::cubeb_stream_params,
            latency_ms: *mut c_uint,
        ) -> c_int,
    >,
    pub get_preferred_sample_rate:
        Option<unsafe extern "C" fn(context: *mut ffi::cubeb, rate: *mut u32) -> c_int>,
    pub enumerate_devices: Option<
        unsafe extern "C" fn(
            context: *mut ffi::cubeb,
            devtype: ffi::cubeb_device_type,
            collection: *mut ffi::cubeb_device_collection,
        ) -> c_int,
    >,
    pub device_collection_destroy: Option<
        unsafe extern "C" fn(
            context: *mut ffi::cubeb,
            collection: *mut ffi::cubeb_device_collection,
        ) -> c_int,
    >,
    pub destroy: Option<unsafe extern "C" fn(context: *mut ffi::cubeb)>,
    pub stream_init: Option<
        unsafe extern "C" fn(
            context: *mut ffi::cubeb,
            stream: *mut *mut ffi::cubeb_stream,
            stream_name: *const c_char,
            input_device: ffi::cubeb_devid,
            input_stream_params: *mut ffi::cubeb_stream_params,
            output_device: ffi::cubeb_devid,
            output_stream_params: *mut ffi::cubeb_stream_params,
            latency: c_uint,
            data_callback: ffi::cubeb_data_callback,
            state_callback: ffi::cubeb_state_callback,
            user_ptr: *mut c_void,
        ) -> c_int,
    >,
    pub stream_destroy: Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream)>,
    pub stream_start: Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_stop: Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_reset_default_device:
        Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_get_position:
        Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, position: *mut u64) -> c_int>,
    pub stream_get_latency:
        Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, latency: *mut u32) -> c_int>,
    pub stream_get_input_latency:
        Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, latency: *mut u32) -> c_int>,
    pub stream_set_volume:
        Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, volumes: c_float) -> c_int>,
    pub stream_get_current_device: Option<
        unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, device: *mut *mut ffi::cubeb_device)
            -> c_int,
    >,
    pub stream_device_destroy: Option<
        unsafe extern "C" fn(stream: *mut ffi::cubeb_stream, device: *mut ffi::cubeb_device)
            -> c_int,
    >,
    pub stream_register_device_changed_callback: Option<
        unsafe extern "C" fn(
            stream: *mut ffi::cubeb_stream,
            device_changed_callback: ffi::cubeb_device_changed_callback,
        ) -> c_int,
    >,
    pub register_device_collection_changed: Option<
        unsafe extern "C" fn(
            context: *mut ffi::cubeb,
            devtype: ffi::cubeb_device_type,
            callback: ffi::cubeb_device_collection_changed_callback,
            user_ptr: *mut c_void,
        ) -> c_int,
    >,
}
