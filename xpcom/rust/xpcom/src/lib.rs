/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate contains the functionality required in order to both implement
//! and call XPCOM methods from rust code.
//!
//! For documentation on how to implement XPCOM methods, see the documentation
//! for the [`xpcom_macros`](../xpcom_macros/index.html) crate.

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

// re-export the xpcom_macros macro
pub use xpcom_macros::xpcom;

// Helper functions and data structures are exported in the root of the crate.
mod base;
pub use base::*;

// Declarative macro to generate XPCOM method stubs.
mod method;
pub use method::*;

// dom::Promise resolving.
mod promise;
pub use promise::*;

mod refptr;
pub use refptr::*;

mod statics;
pub use statics::*;

// XPCOM interface definitions.
pub mod interfaces;

// XPCOM component getters.
pub mod components;

// Implementation details of the xpcom_macros crate.
#[doc(hidden)]
pub mod reexports;
