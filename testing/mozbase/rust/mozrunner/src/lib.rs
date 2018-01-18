#[macro_use] extern crate log;
extern crate mozprofile;
#[cfg(target_os = "windows")]
extern crate winreg;

pub mod runner;

pub use runner::platform::firefox_default_path;
