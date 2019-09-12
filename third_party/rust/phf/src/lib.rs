//! Compile-time generated maps and sets.
//!
//! The `phf::Map` and `phf::Set` types have roughly comparable performance to
//! a standard hash table, but can be generated as compile-time static values.
//!
//! # Usage
//!
//! If the `macros` Cargo feature is enabled, the `phf_map`, `phf_set`,
//! `phf_ordered_map`, and `phf_ordered_set` macros can be used to construct
//! the PHF type. This currently requires a nightly compiler.
//!
//! ```toml
//! [dependencies]
//! phf = { version = "0.7.24", features = ["macros"] }
//! ```
//!
//! ```
//! #![feature(proc_macro_hygiene)]
//!
//! use phf::{phf_map, phf_set};
//!
//! static MY_MAP: phf::Map<&'static str, u32> = phf_map! {
//!     "hello" => 1,
//!     "world" => 2,
//! };
//!
//! static MY_SET: phf::Set<&'static str> = phf_set! {
//!     "hello world",
//!     "hola mundo",
//! };
//!
//! fn main() {
//!     assert_eq!(MY_MAP["hello"], 1);
//!     assert!(MY_SET.contains("hello world"));
//! }
//! ```
//!
//! Alternatively, you can use the phf_codegen crate to generate PHF datatypes
//! in a build script. This method can be used with a stable compiler.
#![doc(html_root_url="https://docs.rs/phf/0.7")]
#![warn(missing_docs)]
#![cfg_attr(feature = "core", no_std)]

#[cfg(not(feature = "core"))]
extern crate std as core;

extern crate phf_shared;
#[cfg(feature = "macros")]
extern crate phf_macros;

#[cfg(feature = "macros")]
pub use phf_macros::*;

use core::ops::Deref;

pub use phf_shared::PhfHash;
#[doc(inline)]
pub use map::Map;
#[doc(inline)]
pub use set::Set;
#[doc(inline)]
pub use ordered_map::OrderedMap;
#[doc(inline)]
pub use ordered_set::OrderedSet;

pub mod map;
pub mod set;
pub mod ordered_map;
pub mod ordered_set;

// WARNING: this is not considered part of phf's public API and is subject to
// change at any time.
//
// Basically Cow, but with the Owned version conditionally compiled
#[doc(hidden)]
pub enum Slice<T: 'static> {
    Static(&'static [T]),
    #[cfg(not(feature = "core"))]
    Dynamic(Vec<T>),
}

impl<T> Deref for Slice<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        match *self {
            Slice::Static(t) => t,
            #[cfg(not(feature = "core"))]
            Slice::Dynamic(ref t) => t,
        }
    }
}
