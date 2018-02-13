// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use channel::{cubeb_channel, CHANNEL_MAX, CUBEB_LAYOUT_MAX};
use format::cubeb_sample_format;
use std::os::raw::{c_long, c_uchar, c_ulong, c_void};
use stream::cubeb_stream_params;

cubeb_enum! {
    pub enum cubeb_mixer_direction {
        CUBEB_MIXER_DIRECTION_DOWNMIX = 0x01,
        CUBEB_MIXER_DIRECTION_UPMIX = 0x02,
    }
}

pub enum cubeb_mixer {}

extern "C" {
    pub fn cubeb_should_upmix(
        stream: *const cubeb_stream_params,
        mixer: *const cubeb_stream_params,
    ) -> bool;
    pub fn cubeb_should_downmix(
        stream: *const cubeb_stream_params,
        mixer: *const cubeb_stream_params,
    ) -> bool;
    pub fn cubeb_should_mix(
        stream: *const cubeb_stream_params,
        mixer: *const cubeb_stream_params,
    ) -> bool;

    pub fn cubeb_mixer_create(format: cubeb_sample_format, direction: c_uchar) -> *mut cubeb_mixer;
    pub fn cubeb_mixer_destroy(mixer: *mut cubeb_mixer);
    pub fn cubeb_mixer_mix(
        mixer: *mut cubeb_mixer,
        frames: c_long,
        input_buffer: *mut c_void,
        input_buffer_length: c_ulong,
        output_buffer: *mut c_void,
        output_buffer_length: c_ulong,
        stream_params: *const cubeb_stream_params,
        mixer_params: *const cubeb_stream_params,
    );

    pub static CHANNEL_INDEX_TO_ORDER:
        [[cubeb_channel; CHANNEL_MAX as usize]; CUBEB_LAYOUT_MAX as usize];
}
