// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate libc;

#[macro_use]
extern crate core_foundation;

#[macro_use]
extern crate bitflags;

#[macro_use]
extern crate foreign_types;

extern crate core_graphics_types;

pub mod base;
pub mod color;
pub mod color_space;
pub mod context;
pub mod data_provider;
#[cfg(target_os = "macos")]
pub mod display;
#[cfg(target_os = "macos")]
pub mod event;
#[cfg(target_os = "macos")]
pub mod event_source;
pub mod font;
pub mod geometry;
pub mod gradient;
#[cfg(target_os = "macos")]
pub mod window;
#[cfg(target_os = "macos")]
pub mod private;
pub mod image;
pub mod path;
pub mod sys;
