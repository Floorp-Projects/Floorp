// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

cubeb_enum! {
    pub enum cubeb_sample_format {
        CUBEB_SAMPLE_S16LE,
        CUBEB_SAMPLE_S16BE,
        CUBEB_SAMPLE_FLOAT32LE,
        CUBEB_SAMPLE_FLOAT32BE,
    }
}

#[cfg(target_endian = "big")]
pub const CUBEB_SAMPLE_S16NE: cubeb_sample_format = CUBEB_SAMPLE_S16BE;
#[cfg(target_endian = "big")]
pub const CUBEB_SAMPLE_FLOAT32NE: cubeb_sample_format = CUBEB_SAMPLE_FLOAT32BE;
#[cfg(target_endian = "little")]
pub const CUBEB_SAMPLE_S16NE: cubeb_sample_format = CUBEB_SAMPLE_S16LE;
#[cfg(target_endian = "little")]
pub const CUBEB_SAMPLE_FLOAT32NE: cubeb_sample_format = CUBEB_SAMPLE_FLOAT32LE;
