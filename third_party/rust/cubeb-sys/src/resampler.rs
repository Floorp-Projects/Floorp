// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use callbacks::cubeb_data_callback;
use std::os::raw::{c_long, c_uint, c_void};
use stream::{cubeb_stream, cubeb_stream_params};

pub enum cubeb_resampler {}

cubeb_enum! {
    pub enum cubeb_resampler_quality {
        CUBEB_RESAMPLER_QUALITY_VOIP,
        CUBEB_RESAMPLER_QUALITY_DEFAULT,
        CUBEB_RESAMPLER_QUALITY_DESKTOP,
    }
}

cubeb_enum! {
    pub enum cubeb_resampler_reclock {
        CUBEB_RESAMPLER_RECLOCK_NONE,
        CUBEB_RESAMPLER_RECLOCK_INPUT,
    }
}

extern "C" {
    pub fn cubeb_resampler_create(
        stream: *mut cubeb_stream,
        input_params: *mut cubeb_stream_params,
        output_params: *mut cubeb_stream_params,
        target_rate: c_uint,
        callback: cubeb_data_callback,
        user_ptr: *mut c_void,
        quality: cubeb_resampler_quality,
        reclock: cubeb_resampler_reclock,
    ) -> *mut cubeb_resampler;

    pub fn cubeb_resampler_fill(
        resampler: *mut cubeb_resampler,
        input_buffer: *mut c_void,
        input_frame_count: *mut c_long,
        output_buffer: *mut c_void,
        output_frames_needed: c_long,
    ) -> c_long;

    pub fn cubeb_resampler_destroy(resampler: *mut cubeb_resampler);
    pub fn cubeb_resampler_latency(resampler: *mut cubeb_resampler) -> c_long;
}
