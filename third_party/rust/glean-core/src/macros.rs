// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![macro_use]

//! Utility macros used in this crate.

/// Unwrap a `Result`s `Ok` value or do the specified action.
///
/// This is an alternative to the question-mark operator (`?`),
/// when the other action should not be to return the error.
macro_rules! unwrap_or {
    ($expr:expr, $or:expr) => {
        match $expr {
            Ok(x) => x,
            Err(_) => {
                $or;
            }
        }
    };
}
