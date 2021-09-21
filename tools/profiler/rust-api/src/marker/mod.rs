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
//! TODO: Explain the marker API once we have them in the following patches.

pub mod options;
