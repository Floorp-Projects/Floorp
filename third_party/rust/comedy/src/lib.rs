// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
#![cfg(windows)]

//! Windows error handling, COM, and handles
//!
//! See macros for examples.

extern crate winapi;

pub mod com;
pub mod error;
pub mod handle;

pub use error::{HResult, ResultExt, Win32Error};
