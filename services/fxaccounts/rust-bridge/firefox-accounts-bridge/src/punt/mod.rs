/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Punt is pretty much a copy-paste of the Golden Gate crate in services/sync/golden_gate
//! but specialized to work on the fxa-client crate.
//! In short it helps run Rust code in a background thread, handle errors and get the results back.
//! There's an effort to factorize these helpers in https://bugzilla.mozilla.org/show_bug.cgi?id=1626703.

pub mod error;
mod punt;
mod task;

pub use task::PuntTask;
