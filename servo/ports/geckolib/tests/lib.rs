/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Disable this entire crate on Windows when Gecko symbols are not available
// as linking would fail:
// https://github.com/rust-lang/rust/pull/44603#issuecomment-338807312
//
// On Linux and OS X linking succeeds anyway.
// Presumably these symbol declarations don’t need to be resolved
// as they’re not used in any code called from this crate.
#![cfg(any(linking_with_gecko, not(windows)))]

extern crate euclid;
extern crate style;

mod piecewise_linear;
