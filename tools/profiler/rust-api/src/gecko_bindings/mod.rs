/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Gecko's C++ bindings for the profiler.

#[allow(
    dead_code,
    non_camel_case_types,
    non_snake_case,
    non_upper_case_globals,
    missing_docs
)]
pub mod structs {
    include!(concat!(env!("OUT_DIR"), "/gecko/bindings.rs"));
}

pub use self::structs as bindings;

mod glue;
pub mod profiling_categories;
