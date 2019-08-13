#![forbid(unsafe_code)]

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
