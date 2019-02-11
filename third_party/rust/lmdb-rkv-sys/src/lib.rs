#![allow(non_camel_case_types)]
#![deny(warnings)]
#![doc(html_root_url = "https://docs.rs/lmdb-rkv-sys/0.8.2")]

extern crate libc;

#[cfg(unix)]
#[allow(non_camel_case_types)]
pub type mode_t = ::libc::mode_t;
#[cfg(windows)]
#[allow(non_camel_case_types)]
pub type mode_t = ::libc::c_int;

pub use constants::*;
pub use ffi::*;

mod ffi;
mod constants;
