// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

/// An extension trait which allows the implementation of converting
/// void* buffers from libcubeb-sys into rust slices of the appropriate
/// type.
pub trait Sample: Send + Copy {
    /// Map f32 in range [-1,1] to sample type
    fn from_float(_: f32) -> Self;
}

impl Sample for i16 {
    fn from_float(x: f32) -> i16 {
        (x * f32::from(i16::max_value())) as i16
    }
}

impl Sample for f32 {
    fn from_float(x: f32) -> f32 {
        x
    }
}
