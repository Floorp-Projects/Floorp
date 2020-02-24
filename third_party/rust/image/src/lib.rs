//! This crate provides native rust implementations of
//! image encoders and decoders and basic image manipulation
//! functions.

#![warn(missing_docs)]
#![warn(unused_qualifications)]
#![deny(unreachable_pub)]
#![deny(deprecated)]
#![deny(missing_copy_implementations)]
#![cfg_attr(all(test, feature = "benchmarks"), feature(test))]
// it's a bit of a pain otherwise
#![allow(clippy::many_single_char_names)]

#[cfg(all(test, feature = "benchmarks"))]
extern crate test;

#[cfg(test)]
#[macro_use]
extern crate quickcheck;

use std::io::Write;

pub use crate::color::{ColorType, ExtendedColorType};

pub use crate::color::{Luma, LumaA, Rgb, Rgba, Bgr, Bgra};

pub use crate::error::{ImageError, ImageResult};

pub use crate::image::{AnimationDecoder,
                GenericImage,
                GenericImageView,
                ImageDecoder,
                ImageDecoderExt,
                ImageEncoder,
                ImageFormat,
                ImageOutputFormat,
                Progress,
                // Iterators
                Pixels,
                SubImage};

pub use crate::buffer::{ConvertBuffer,
                 GrayAlphaImage,
                 GrayImage,
                 // Image types
                 ImageBuffer,
                 Pixel,
                 RgbImage,
                 RgbaImage,
                 };

pub use crate::flat::FlatSamples;

// Traits
pub use crate::traits::Primitive;

// Opening and loading images
pub use crate::io::free_functions::{guess_format, load};
pub use crate::dynimage::{load_from_memory, load_from_memory_with_format, open,
                   save_buffer, save_buffer_with_format, image_dimensions};

pub use crate::dynimage::DynamicImage;

pub use crate::animation::{Delay, Frame, Frames};

// More detailed error type
pub mod error;

// Math utils
pub mod math;

// Image processing functions
pub mod imageops;

// Io bindings
pub mod io;

// Buffer representations for ffi.
pub mod flat;

// Image codecs
#[cfg(feature = "bmp")]
pub mod bmp;
#[cfg(feature = "dds")]
pub mod dds;
#[cfg(feature = "dxt")]
pub mod dxt;
#[cfg(feature = "gif")]
pub mod gif;
#[cfg(feature = "hdr")]
pub mod hdr;
#[cfg(feature = "ico")]
pub mod ico;
#[cfg(feature = "jpeg")]
pub mod jpeg;
#[cfg(feature = "png")]
pub mod png;
#[cfg(feature = "pnm")]
pub mod pnm;
#[cfg(feature = "tga")]
pub mod tga;
#[cfg(feature = "tiff")]
pub mod tiff;
#[cfg(feature = "webp")]
pub mod webp;

mod animation;
mod buffer;
mod color;
mod dynimage;
mod image;
mod traits;
mod utils;

// Can't use the macro-call itself within the `doc` attribute. So force it to eval it as part of
// the macro invocation.
//
// The inspiration for the macro and implementation is from
// <https://github.com/GuillaumeGomez/doc-comment>
//
// MIT License
//
// Copyright (c) 2018 Guillaume Gomez
macro_rules! insert_as_doc {
    { $content:expr } => {
        #[doc = $content] extern { }
    }
}

// Provides the README.md as doc, to ensure the example works!
insert_as_doc!(include_str!("../README.md"));

// Copies data from `src` to `dst`
//
// Panics if the length of `dst` is less than the length of `src`.
#[inline]
fn copy_memory(src: &[u8], mut dst: &mut [u8]) {
    let len_src = src.len();
    assert!(dst.len() >= len_src);
    dst.write_all(src).unwrap();
}
