/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! ## Gecko profiler marker support
//!
//! This marker API has a few different functions that you can use to mark a part of your code.
//! There are three main marker functions to use from Rust: [`add_untyped_marker`],
//! [`add_text_marker`] and [`add_marker`]. They are similar to what we have on
//! the C++ side. Please take a look at the marker documentation in the Firefox
//! source docs to learn more about them:
//! https://firefox-source-docs.mozilla.org/tools/profiler/markers-guide.html
//!
//! ### Simple marker without any additional data
//!
//! The simplest way to add a marker without any additional information is the
//! [`add_untyped_marker`] API. You can use it to mark a part of the code with
//! only a name. E.g.:
//!
//! ```
//! gecko_profiler::add_untyped_marker(
//!     // Name of the marker as a string.
//!     "Marker Name",
//!     // Category with an optional sub-category.
//!     gecko_profiler_category!(Graphics, DisplayListBuilding),
//!     // MarkerOptions that keeps options like marker timing and marker stack.
//!     Default::default(),
//! );
//! ```
//!
//! Please see the [`gecko_profiler_category!`], [`MarkerOptions`],[`MarkerTiming`]
//! and [`MarkerStack`] to learn more about these.
//!
//! You can also give explicit [`MarkerOptions`] value like these:
//!
//! ```
//! // With both timing and stack fields:
//! MarkerOptions { timing: MarkerTiming::instant_now(), stack: MarkerStack::Full }
//! // Or with some fields as default:
//! MarkerOptions { timing: MarkerTiming::instant_now(), ..Default::default() }
//! ```
//!
//! ### Marker with only an additional text for more information:
//!
//! The next and slightly more advanced API is [`add_text_marker`].
//! This is used to add a marker name + a string value for extra information.
//! E.g.:
//!
//! ```
//! let info = "info about this marker";
//! ...
//! gecko_profiler::add_text_marker(
//!     // Name of the marker as a string.
//!     "Marker Name",
//!     // Category with an optional sub-category.
//!     gecko_profiler_category!(DOM),
//!     // MarkerOptions that keeps options like marker timing and marker stack.
//!     MarkerOptions {
//!         timing: MarkerTiming::instant_now(),
//!         ..Default::default()
//!     },
//!     // Additional information as a string.
//!     info,
//! );
//! ```
//!
//! TODO: Explain the marker API once we have them in the following patches.

pub mod options;
pub mod schema;

use crate::gecko_bindings::{bindings, profiling_categories::ProfilingCategoryPair};
use crate::marker::options::MarkerOptions;
use std::os::raw::c_char;

/// Marker API to add a new simple marker without any payload.
/// Please see the module documentation on how to add a marker with this API.
pub fn add_untyped_marker(name: &str, category: ProfilingCategoryPair, mut options: MarkerOptions) {
    if !crate::profiler_state::can_accept_markers() {
        // Nothing to do.
        return;
    }

    unsafe {
        bindings::gecko_profiler_add_marker_untyped(
            name.as_ptr() as *const c_char,
            name.len(),
            category.to_cpp_enum_value(),
            options.timing.0.as_mut_ptr(),
            options.stack,
        )
    }
}

/// Marker API to add a new marker with additional text for details.
/// Please see the module documentation on how to add a marker with this API.
pub fn add_text_marker(
    name: &str,
    category: ProfilingCategoryPair,
    mut options: MarkerOptions,
    text: &str,
) {
    if !crate::profiler_state::can_accept_markers() {
        // Nothing to do.
        return;
    }

    unsafe {
        bindings::gecko_profiler_add_marker_text(
            name.as_ptr() as *const c_char,
            name.len(),
            category.to_cpp_enum_value(),
            options.timing.0.as_mut_ptr(),
            options.stack,
            text.as_ptr() as *const c_char,
            text.len(),
        )
    }
}
