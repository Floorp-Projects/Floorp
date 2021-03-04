#![doc(html_root_url = "https://docs.rs/spirv_headers/1.5/")]

//! The SPIR-V header.
//!
//! This crate contains Rust definitions of all SPIR-V structs, enums,
//! and constants.
//!
//! The version of this crate is the version of SPIR-V it contains.

#![allow(non_camel_case_types)]
#![cfg_attr(rustfmt, rustfmt_skip)]

use bitflags::bitflags;
use num_traits;

include!("autogen_spirv.rs");
