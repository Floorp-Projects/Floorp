/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! An Au is an "App Unit" and represents 1/60th of a CSS pixel. It was
//! originally proposed in 2002 as a standard unit of measure in Gecko.
//! See <https://bugzilla.mozilla.org/show_bug.cgi?id=177805> for more info.

extern crate num_traits;
extern crate serde;

mod app_unit;

pub use app_unit::{Au, MIN_AU, MAX_AU, AU_PER_PX};
