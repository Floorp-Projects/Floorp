#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

#[cfg(fullback_bindings)]
include!("fullback_coreaudio.rs");

#[cfg(not(fullback_bindings))]
include!(concat!(env!("OUT_DIR"), "/coreaudio.rs"));
