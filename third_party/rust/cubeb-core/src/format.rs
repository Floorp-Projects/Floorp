// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;

#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum SampleFormat {
    S16LE,
    S16BE,
    Float32LE,
    Float32BE,
    // Maps to the platform native endian
    S16NE,
    Float32NE,
}

impl From<ffi::cubeb_sample_format> for SampleFormat {
    fn from(x: ffi::cubeb_sample_format) -> SampleFormat {
        match x {
            ffi::CUBEB_SAMPLE_S16LE => SampleFormat::S16LE,
            ffi::CUBEB_SAMPLE_S16BE => SampleFormat::S16BE,
            ffi::CUBEB_SAMPLE_FLOAT32LE => SampleFormat::Float32LE,
            ffi::CUBEB_SAMPLE_FLOAT32BE => SampleFormat::Float32BE,
            // TODO: Implement TryFrom
            _ => SampleFormat::S16NE,
        }
    }
}

impl From<SampleFormat> for ffi::cubeb_sample_format {
    fn from(x: SampleFormat) -> Self {
        use SampleFormat::*;
        match x {
            S16LE => ffi::CUBEB_SAMPLE_S16LE,
            S16BE => ffi::CUBEB_SAMPLE_S16BE,
            Float32LE => ffi::CUBEB_SAMPLE_FLOAT32LE,
            Float32BE => ffi::CUBEB_SAMPLE_FLOAT32BE,
            S16NE => ffi::CUBEB_SAMPLE_S16NE,
            Float32NE => ffi::CUBEB_SAMPLE_FLOAT32NE,
        }
    }
}
