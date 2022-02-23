#![deny(warnings)]
#![allow(non_camel_case_types)]
#![allow(clippy::all)]
#![doc(html_root_url = "https://docs.rs/lmdb-rkv-sys/0.11.0")]

extern crate libc;

#[cfg(unix)]
#[allow(non_camel_case_types)]
pub type mdb_mode_t = ::libc::mode_t;
#[cfg(windows)]
#[allow(non_camel_case_types)]
pub type mdb_mode_t = ::libc::c_int;

#[cfg(unix)]
#[allow(non_camel_case_types)]
pub type mdb_filehandle_t = ::libc::c_int;
#[cfg(windows)]
#[allow(non_camel_case_types)]
pub type mdb_filehandle_t = *mut ::libc::c_void;

include!("bindings.rs");
