//! The SPIR-V header.
//!
//! This crate contains Rust definitions of all SPIR-V structs, enums,
//! and constants.
//!
//! The version of this crate is the version of SPIR-V it contains.

#![no_std]
#![allow(non_camel_case_types)]
#![cfg_attr(rustfmt, rustfmt_skip)]

use bitflags::bitflags;

include!("autogen_spirv.rs");
