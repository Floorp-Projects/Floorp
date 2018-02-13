// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::{fmt, mem};
use std::os::raw::{c_int, c_uint};

cubeb_enum! {
    pub enum cubeb_channel : c_int {
        CHANNEL_INVALID = -1,
        CHANNEL_MONO = 0,
        CHANNEL_LEFT,
        CHANNEL_RIGHT,
        CHANNEL_CENTER,
        CHANNEL_LS,
        CHANNEL_RS,
        CHANNEL_RLS,
        CHANNEL_RCENTER,
        CHANNEL_RRS,
        CHANNEL_LFE,
        CHANNEL_UNMAPPED,
        CHANNEL_MAX = 256,
    }
}

cubeb_enum! {
    pub enum cubeb_channel_layout {
        CUBEB_LAYOUT_UNDEFINED,

        CUBEB_LAYOUT_DUAL_MONO,
        CUBEB_LAYOUT_DUAL_MONO_LFE,
        CUBEB_LAYOUT_MONO,
        CUBEB_LAYOUT_MONO_LFE,
        CUBEB_LAYOUT_STEREO,
        CUBEB_LAYOUT_STEREO_LFE,
        CUBEB_LAYOUT_3F,
        CUBEB_LAYOUT_3F_LFE,
        CUBEB_LAYOUT_2F1,
        CUBEB_LAYOUT_2F1_LFE,
        CUBEB_LAYOUT_3F1,
        CUBEB_LAYOUT_3F1_LFE,
        CUBEB_LAYOUT_2F2,
        CUBEB_LAYOUT_2F2_LFE,
        CUBEB_LAYOUT_3F2,
        CUBEB_LAYOUT_3F2_LFE,
        CUBEB_LAYOUT_3F3R_LFE,
        CUBEB_LAYOUT_3F4_LFE,
        CUBEB_LAYOUT_MAX,
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct cubeb_channel_map {
    pub channels: c_uint,
    pub map: [cubeb_channel; CHANNEL_MAX as usize],
}

impl Default for cubeb_channel_map {
    fn default() -> Self {
        unsafe { mem::zeroed() }
    }
}

// Explicit Debug impl to work around bug in ctest
impl fmt::Debug for cubeb_channel_map {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("cubeb_channel_map")
            .field("channels", &self.channels)
            .field("map", &self.map.iter().take(self.channels as usize))
            .finish()
    }
}

extern "C" {
    pub fn cubeb_channel_map_to_layout(
        channel_map: *const cubeb_channel_map,
    ) -> cubeb_channel_layout;
}
