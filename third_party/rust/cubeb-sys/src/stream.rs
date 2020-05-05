// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use callbacks::cubeb_device_changed_callback;
use channel::cubeb_channel_layout;
use device::cubeb_device;
use format::cubeb_sample_format;
use std::{fmt, mem};
use std::os::raw::{c_float, c_int, c_uint, c_void};

cubeb_enum! {
    pub enum cubeb_stream_prefs {
        CUBEB_STREAM_PREF_NONE = 0x00,
        CUBEB_STREAM_PREF_LOOPBACK = 0x01,
        CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING = 0x02,
        CUBEB_STREAM_PREF_VOICE = 0x04,
    }
}

cubeb_enum! {
    pub enum cubeb_state {
        CUBEB_STATE_STARTED,
        CUBEB_STATE_STOPPED,
        CUBEB_STATE_DRAINED,
        CUBEB_STATE_ERROR,
    }
}

pub enum cubeb_stream {}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct cubeb_stream_params {
    pub format: cubeb_sample_format,
    pub rate: c_uint,
    pub channels: c_uint,
    pub layout: cubeb_channel_layout,
    pub prefs: cubeb_stream_prefs,
}

impl Default for cubeb_stream_params {
    fn default() -> Self {
        unsafe { mem::zeroed() }
    }
}

// Explicit Debug impl to work around bug in ctest
impl fmt::Debug for cubeb_stream_params {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_stream_params")
            .field("format", &self.format)
            .field("rate", &self.rate)
            .field("channels", &self.channels)
            .field("layout", &self.layout)
            .field("prefs", &self.prefs)
            .finish()
    }
}

extern "C" {
    pub fn cubeb_stream_destroy(stream: *mut cubeb_stream);
    pub fn cubeb_stream_start(stream: *mut cubeb_stream) -> c_int;
    pub fn cubeb_stream_stop(stream: *mut cubeb_stream) -> c_int;
    pub fn cubeb_stream_reset_default_device(stream: *mut cubeb_stream) -> c_int;
    pub fn cubeb_stream_get_position(stream: *mut cubeb_stream, position: *mut u64) -> c_int;
    pub fn cubeb_stream_get_latency(stream: *mut cubeb_stream, latency: *mut c_uint) -> c_int;
    pub fn cubeb_stream_get_input_latency(stream: *mut cubeb_stream, latency: *mut c_uint) -> c_int;
    pub fn cubeb_stream_set_volume(stream: *mut cubeb_stream, volume: c_float) -> c_int;
    pub fn cubeb_stream_get_current_device(
        stream: *mut cubeb_stream,
        device: *mut *mut cubeb_device,
    ) -> c_int;
    pub fn cubeb_stream_device_destroy(
        stream: *mut cubeb_stream,
        devices: *mut cubeb_device,
    ) -> c_int;
    pub fn cubeb_stream_register_device_changed_callback(
        stream: *mut cubeb_stream,
        device_changed_callback: cubeb_device_changed_callback,
    ) -> c_int;
    pub fn cubeb_stream_user_ptr(stream: *mut cubeb_stream) -> *mut c_void;
}
