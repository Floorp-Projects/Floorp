/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]

// Note that in the future it might make sense to also add a trait for
// an Interruptable, but we don't need this abstraction now and it's unclear
// if we ever will.

/// Represents the state of something that may be interrupted. Decoupled from
/// the interrupt mechanics so that things which want to check if they have been
/// interrupted are simpler.
pub trait Interruptee {
    fn was_interrupted(&self) -> bool;

    fn err_if_interrupted(&self) -> Result<(), Interrupted> {
        if self.was_interrupted() {
            return Err(Interrupted);
        }
        Ok(())
    }
}

/// A convenience implementation, should only be used in tests.
pub struct NeverInterrupts;

impl Interruptee for NeverInterrupts {
    #[inline]
    fn was_interrupted(&self) -> bool {
        false
    }
}

/// The error returned by err_if_interrupted.
#[derive(Debug, Clone, PartialEq)]
pub struct Interrupted;

impl std::fmt::Display for Interrupted {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("The operation was interrupted")
    }
}

impl std::error::Error for Interrupted {}
