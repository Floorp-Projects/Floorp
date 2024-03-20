/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Standard library wrapper (for mocking in tests).

pub use std::*;

#[cfg_attr(not(mock), path = "mock_stub.rs")]
pub mod mock;

#[cfg(mock)]
pub mod env;
#[cfg(mock)]
pub mod fs;
#[cfg(mock)]
pub mod net;
#[cfg(mock)]
pub mod path;
#[cfg(mock)]
pub mod process;
#[cfg(mock)]
pub mod thread;
#[cfg(mock)]
pub mod time;
