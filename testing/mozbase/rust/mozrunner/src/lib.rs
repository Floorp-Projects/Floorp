#![forbid(unsafe_code)]
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate log;
#[cfg(target_os = "macos")]
extern crate dirs;
extern crate mozprofile;
#[cfg(target_os = "macos")]
extern crate plist;
#[cfg(target_os = "windows")]
extern crate winreg;

pub mod firefox_args;
pub mod path;
pub mod runner;

pub use crate::runner::platform::firefox_default_path;
