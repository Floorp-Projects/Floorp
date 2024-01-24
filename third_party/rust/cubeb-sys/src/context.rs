// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use callbacks::{cubeb_data_callback, cubeb_state_callback};
use device::cubeb_devid;
use std::os::raw::{c_char, c_int, c_uint, c_void};
use stream::{cubeb_input_processing_params, cubeb_stream, cubeb_stream_params};

pub enum cubeb {}

extern "C" {
    pub fn cubeb_init(
        context: *mut *mut cubeb,
        context_name: *const c_char,
        backend_name: *const c_char,
    ) -> c_int;
    pub fn cubeb_get_backend_id(context: *mut cubeb) -> *const c_char;
    pub fn cubeb_get_max_channel_count(context: *mut cubeb, max_channels: *mut c_uint) -> c_int;
    pub fn cubeb_get_min_latency(
        context: *mut cubeb,
        params: *mut cubeb_stream_params,
        latency_frames: *mut c_uint,
    ) -> c_int;
    pub fn cubeb_get_preferred_sample_rate(context: *mut cubeb, rate: *mut c_uint) -> c_int;
    pub fn cubeb_get_supported_input_processing_params(
        context: *mut cubeb,
        params: *mut cubeb_input_processing_params,
    ) -> c_int;
    pub fn cubeb_destroy(context: *mut cubeb);
    pub fn cubeb_stream_init(
        context: *mut cubeb,
        stream: *mut *mut cubeb_stream,
        stream_name: *const c_char,
        input_device: cubeb_devid,
        input_stream_params: *mut cubeb_stream_params,
        output_device: cubeb_devid,
        output_stream_params: *mut cubeb_stream_params,
        latency_frames: c_uint,
        data_callback: cubeb_data_callback,
        state_callback: cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> c_int;
}
