// Copyright ⓒ 2015-2017 Benjamin Sago, Kevin Knapp, and [`term_size` contributors.](https://github.com/kbknapp/term_size-rs/blob/master/CONTRIBUTORS.md).
//
// Licensed under either of
//
// * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
// * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)
//
// at your option. Unless specifically stated otherwise, all contributions will be licensed in the same manner.

// The following was originally taken and adapated from exa source
// repo: https://github.com/ogham/exa
// commit: b9eb364823d0d4f9085eb220233c704a13d0f611
// license: MIT - Copyright (c) 2014 Benjamin Sago

//! System calls for getting the terminal size.
//!
//! Getting the terminal size is performed using an ioctl command that takes
//! the file handle to the terminal -- which in this case, is stdout -- and
//! populates a structure containing the values.
//!
//! The size is needed when the user wants the output formatted into columns:
//! the default grid view, or the hybrid grid-details view.
//!
//! # Example
//!
//! To get the dimensions of your terminal window, simply use the following:
//!
//! ```no_run
//! # use term_size;
//! if let Some((w, h)) = term_size::dimensions() {
//!     println!("Width: {}\nHeight: {}", w, h);
//! } else {
//!     println!("Unable to get term size :(")
//! }
//! ```
#![doc(html_root_url = "https://docs.rs/term_size/1.0.0-beta1")]
#![deny(
    missing_docs,
    missing_debug_implementations,
    missing_copy_implementations,
    trivial_casts,
    unused_import_braces,
    unused_allocation,
    unused_qualifications,
    trivial_numeric_casts
)]
#![cfg_attr(not(feature = "nightly"), forbid(unstable_features))]

#[cfg(not(target_os = "windows"))]
extern crate libc;
#[cfg(target_os = "windows")]
extern crate winapi;

// A facade to allow exposing functions depending on the platform
mod platform;
pub use platform::{dimensions, dimensions_stderr, dimensions_stdin, dimensions_stdout};
