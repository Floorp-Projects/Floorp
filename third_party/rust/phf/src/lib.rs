//! Compile-time generated maps and sets.
//!
//! The `phf::Map` and `phf::Set` types have roughly comparable performance to
//! a standard hash table, but can be generated as compile-time static values.
//!
//! # Usage
//!
//! If the `macros` Cargo feature is enabled, the `phf_map`, `phf_set`,
//! `phf_ordered_map`, and `phf_ordered_set` macros can be used to construct
//! the PHF type. This method can be used with a stable compiler
//! (`rustc` version 1.30+)
//!
//! ```toml
//! [dependencies]
//! phf = { version = "0.7.24", features = ["macros"] }
//! ```
//!
//! ```
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
//! (Alternatively, you can use the phf_codegen crate to generate PHF datatypes
//! in a build script)
#![doc(html_root_url="https://docs.rs/phf/0.7")]
#![warn(missing_docs)]
#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "std")]
extern crate std as core;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`Map`].
///
/// Requires the `"macros"` feature.
///
/// # Example
///
/// ```rust,edition2018
/// use ::phf::{phf_map, Map};
///
/// static MY_MAP: Map<&'static str, u32> = phf_map! {
///     "hello" => 1,
///     "world" => 2,
/// };
///
/// fn main ()
/// {
///     assert_eq!(MY_MAP["hello"], 1);
/// }
/// ```
#[::proc_macro_hack::proc_macro_hack]
pub use phf_macros:: phf_map;

#[cfg(feature = "macros")]
/// Macro to create a `static` (compile-time) [`Set`].
///
/// Requires the `"macros"` feature.
///
/// # Example
///
/// ```rust,edition2018
/// use ::phf::{phf_set, Set};
///
/// static MY_SET: Set<&'static str> = phf_set! {
///     "hello world",
///     "hola mundo",
/// };
///
/// fn main ()
/// {
///     assert!(MY_SET.contains("hello world"));
/// }
/// ```
#[::proc_macro_hack::proc_macro_hack]
pub use phf_macros::phf_set;

use core::ops::Deref;

pub use phf_shared::PhfHash;
#[doc(inline)]
pub use self::map::Map;
#[doc(inline)]
pub use self::set::Set;

pub mod map;
pub mod set;

// WARNING: this is not considered part of phf's public API and is subject to
// change at any time.
//
// Basically Cow, but with the Owned version conditionally compiled
#[doc(hidden)]
pub enum Slice<T: 'static> {
    Static(&'static [T]),
    #[cfg(feature = "std")]
    Dynamic(Vec<T>),
}

impl<T> Deref for Slice<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        match *self {
            Slice::Static(t) => t,
            #[cfg(feature = "std")]
            Slice::Dynamic(ref t) => t,
        }
    }
}
