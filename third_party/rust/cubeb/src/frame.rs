//! Frame utilities

use std::{mem, slice};

/// A `Frame` is a collection of samples which have a a specific
/// layout represented by `ChannelLayout`
pub trait Frame {}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A monaural frame.
pub struct MonoFrame<T> {
    /// Mono channel
    pub m: T,
}

impl<T> Frame for MonoFrame<T> {}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// A stereo frame.
pub struct StereoFrame<T> {
    /// Left channel
    pub l: T,
    /// Right channel
    pub r: T,
}

impl<T> Frame for StereoFrame<T> {}

pub unsafe fn frame_from_bytes<F: Frame>(bytes: &[u8]) -> &[F] {
    debug_assert_eq!(bytes.len() % mem::size_of::<F>(), 0);
    slice::from_raw_parts(
        bytes.as_ptr() as *const F,
        bytes.len() / mem::size_of::<F>(),
    )
}

pub unsafe fn frame_from_bytes_mut<F: Frame>(bytes: &mut [u8]) -> &mut [F] {
    debug_assert!(bytes.len() % mem::size_of::<F>() == 0);
    slice::from_raw_parts_mut(bytes.as_ptr() as *mut F, bytes.len() / mem::size_of::<F>())
}
