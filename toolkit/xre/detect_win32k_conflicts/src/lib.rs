/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Crate to detect Windows Exploit Protection mitigations that may interfere with Win32k
//! Lockdown

#![deny(missing_docs)]

mod detect;
mod error;
mod ffi;
mod registry;

pub use self::detect::get_conflicting_mitigations;
pub use self::detect::ConflictingMitigationStatus;
pub use self::error::DetectConflictError;
pub use self::ffi::detect_win32k_conflicting_mitigations;
