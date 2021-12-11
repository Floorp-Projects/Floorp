// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use channel::cubeb_channel_layout;
use format::cubeb_sample_format;
use std::os::raw::{c_int, c_uint, c_void};

pub enum cubeb_mixer {}

extern "C" {
    pub fn cubeb_mixer_create(
        format: cubeb_sample_format,
        in_channels: u32,
        in_layout: cubeb_channel_layout,
        out_channels: u32,
        out_layout: cubeb_channel_layout,
    ) -> *mut cubeb_mixer;
    pub fn cubeb_mixer_destroy(mixer: *mut cubeb_mixer);
    pub fn cubeb_mixer_mix(
        mixer: *mut cubeb_mixer,
        frames: usize,
        input_buffer: *const c_void,
        input_buffer_length: usize,
        output_buffer: *mut c_void,
        output_buffer_length: usize,
    ) -> c_int;

    pub fn cubeb_channel_layout_nb_channels(channel_layout: cubeb_channel_layout) -> c_uint;
}
