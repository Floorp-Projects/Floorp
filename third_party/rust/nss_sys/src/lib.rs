/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]
#![allow(non_camel_case_types, non_upper_case_globals, non_snake_case)]

mod bindings;
pub use bindings::*;

// So we link against the SQLite lib imported by parent crates
// such as places and logins.
#[allow(unused_extern_crates)]
#[cfg(any(not(feature = "gecko"), __appsvc_ci_hack))]
extern crate libsqlite3_sys;
