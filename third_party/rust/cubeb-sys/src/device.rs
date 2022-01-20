// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use callbacks::cubeb_device_collection_changed_callback;
use context::cubeb;
use std::{fmt, mem};
use std::os::raw::{c_char, c_int, c_uint, c_void};

cubeb_enum! {
    pub enum cubeb_device_fmt {
        CUBEB_DEVICE_FMT_S16LE          = 0x0010,
        CUBEB_DEVICE_FMT_S16BE          = 0x0020,
        CUBEB_DEVICE_FMT_F32LE          = 0x1000,
        CUBEB_DEVICE_FMT_F32BE          = 0x2000,
    }
}

#[cfg(target_endian = "big")]
pub const CUBEB_DEVICE_FMT_S16NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_S16BE;
#[cfg(target_endian = "big")]
pub const CUBEB_DEVICE_FMT_F32NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_F32BE;
#[cfg(target_endian = "little")]
pub const CUBEB_DEVICE_FMT_S16NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_S16LE;
#[cfg(target_endian = "little")]
pub const CUBEB_DEVICE_FMT_F32NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_F32LE;

pub const CUBEB_DEVICE_FMT_S16_MASK: cubeb_device_fmt =
    CUBEB_DEVICE_FMT_S16LE | CUBEB_DEVICE_FMT_S16BE;
pub const CUBEB_DEVICE_FMT_F32_MASK: cubeb_device_fmt =
    CUBEB_DEVICE_FMT_F32LE | CUBEB_DEVICE_FMT_F32BE;
pub const CUBEB_DEVICE_FMT_ALL: cubeb_device_fmt =
    CUBEB_DEVICE_FMT_S16_MASK | CUBEB_DEVICE_FMT_F32_MASK;

cubeb_enum! {
    pub enum cubeb_device_pref  {
        CUBEB_DEVICE_PREF_NONE          = 0x00,
        CUBEB_DEVICE_PREF_MULTIMEDIA    = 0x01,
        CUBEB_DEVICE_PREF_VOICE         = 0x02,
        CUBEB_DEVICE_PREF_NOTIFICATION  = 0x04,
        CUBEB_DEVICE_PREF_ALL           = 0x0F,
    }
}

cubeb_enum! {
    pub enum cubeb_device_state {
        CUBEB_DEVICE_STATE_DISABLED,
        CUBEB_DEVICE_STATE_UNPLUGGED,
        CUBEB_DEVICE_STATE_ENABLED,
    }
}
cubeb_enum! {
    pub enum cubeb_device_type {
        CUBEB_DEVICE_TYPE_UNKNOWN,
        CUBEB_DEVICE_TYPE_INPUT,
        CUBEB_DEVICE_TYPE_OUTPUT,
    }
}

pub type cubeb_devid = *const c_void;

#[repr(C)]
pub struct cubeb_device {
    pub output_name: *mut c_char,
    pub input_name: *mut c_char,
}

// Explicit Debug impl to work around bug in ctest
impl Default for cubeb_device {
    fn default() -> Self {
        unsafe { mem::zeroed() }
    }
}

impl fmt::Debug for cubeb_device {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_device")
            .field("output_name", &self.output_name)
            .field("input_name", &self.input_name)
            .finish()
    }
}

#[repr(C)]
pub struct cubeb_device_collection {
    pub device: *mut cubeb_device_info,
    pub count: usize,
}

impl Default for cubeb_device_collection {
    fn default() -> Self {
        unsafe { mem::zeroed() }
    }
}

impl fmt::Debug for cubeb_device_collection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_device_collection")
            .field("device", &self.device)
            .field("count", &self.count)
            .finish()
    }
}

#[repr(C)]
pub struct cubeb_device_info {
    pub devid: cubeb_devid,
    pub device_id: *const c_char,
    pub friendly_name: *const c_char,
    pub group_id: *const c_char,
    pub vendor_name: *const c_char,

    pub device_type: cubeb_device_type,
    pub state: cubeb_device_state,
    pub preferred: cubeb_device_pref,

    pub format: cubeb_device_fmt,
    pub default_format: cubeb_device_fmt,
    pub max_channels: c_uint,
    pub default_rate: c_uint,
    pub max_rate: c_uint,
    pub min_rate: c_uint,

    pub latency_lo: c_uint,
    pub latency_hi: c_uint,
}

impl Default for cubeb_device_info {
    fn default() -> Self {
        unsafe { mem::zeroed() }
    }
}

impl fmt::Debug for cubeb_device_info {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_device_info")
            .field("devid", &self.devid)
            .field("device_id", &self.device_id)
            .field("friendly_name", &self.friendly_name)
            .field("group_id", &self.group_id)
            .field("vendor_name", &self.vendor_name)
            .field("device_type", &self.device_type)
            .field("state", &self.state)
            .field("preferred", &self.preferred)
            .field("format", &self.format)
            .field("default_format", &self.default_format)
            .field("max_channels", &self.max_channels)
            .field("default_rate", &self.default_rate)
            .field("max_rate", &self.max_rate)
            .field("min_rate", &self.min_rate)
            .field("latency_lo", &self.latency_lo)
            .field("latency_hi", &self.latency_hi)
            .finish()
    }
}

extern "C" {
    pub fn cubeb_enumerate_devices(
        context: *mut cubeb,
        devtype: cubeb_device_type,
        collection: *mut cubeb_device_collection,
    ) -> c_int;
    pub fn cubeb_device_collection_destroy(
        context: *mut cubeb,
        collection: *mut cubeb_device_collection,
    ) -> c_int;
    pub fn cubeb_register_device_collection_changed(
        context: *mut cubeb,
        devtype: cubeb_device_type,
        callback: cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> c_int;
}
