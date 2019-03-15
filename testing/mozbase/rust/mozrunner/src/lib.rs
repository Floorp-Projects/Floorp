#[macro_use]
extern crate log;
extern crate mozprofile;
#[cfg(target_os = "windows")]
extern crate winreg;
#[cfg(target_os = "macos")]
extern crate dirs;
#[cfg(target_os = "macos")]
extern crate plist;

pub mod firefox_args;
pub mod path;
pub mod runner;

pub use crate::runner::platform::firefox_default_path;
