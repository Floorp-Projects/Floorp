//! # PNG encoder and decoder
//! This crate contains a PNG encoder and decoder. It supports reading of single lines or whole frames.
//! ## The decoder
//! The most important types for decoding purposes are [`Decoder`](struct.Decoder.html) and
//! [`Reader`](struct.Reader.html). They both wrap a `std::io::Read`.
//! `Decoder` serves as a builder for `Reader`. Calling `Decoder::read_info` reads from the `Read` until the
//! image data is reached.
//! ### Using the decoder
//!     use std::fs::File;
//!
//!     // The decoder is a build for reader and can be used to set various decoding options
//!     // via `Transformations`. The default output transformation is `Transformations::EXPAND
//!     // | Transformations::STRIP_ALPHA`.
//!     let decoder = png::Decoder::new(File::open("tests/pngsuite/basi0g01.png").unwrap());
//!     let (info, mut reader) = decoder.read_info().unwrap();
//!     // Allocate the output buffer.
//!     let mut buf = vec![0; info.buffer_size()];
//!     // Read the next frame. Currently this function should only called once.
//!     // The default options
//!     reader.next_frame(&mut buf).unwrap();
//! ## Encoder
//! ### Using the encoder
//! ```ignore
//! // For reading and opening files
//! use std::path::Path;
//! use std::fs::File;
//! use std::io::BufWriter;
//! // To use encoder.set()
//! use png::HasParameters;
//!
//! let path = Path::new(r"/path/to/image.png");
//! let file = File::create(path).unwrap();
//! let ref mut w = BufWriter::new(file);
//!
//! let mut encoder = png::Encoder::new(w, 2, 1); // Width is 2 pixels and height is 1.
//! encoder.set(png::ColorType::RGBA).set(png::BitDepth::Eight);
//! let mut writer = encoder.write_header().unwrap();
//!
//! let data = [255, 0, 0, 255, 0, 0, 0, 255]; // An array containing a RGBA sequence. First pixel is red and second pixel is black.
//! writer.write_image_data(&data).unwrap(); // Save
//! ```
//!
//#![cfg_attr(test, feature(test))]

#[macro_use] extern crate bitflags;

pub mod chunk;
mod decoder;
#[cfg(feature = "png-encoding")]
mod encoder;
mod filter;
mod traits;
mod common;
mod utils;

pub use crate::common::*;
pub use crate::decoder::{Decoder, Reader, OutputInfo, StreamingDecoder, Decoded, DecodingError, Limits};
#[cfg(feature = "png-encoding")]
pub use crate::encoder::{Encoder, Writer, StreamWriter, EncodingError};
pub use crate::filter::FilterType;
