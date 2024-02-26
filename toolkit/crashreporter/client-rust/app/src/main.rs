/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Use the WINDOWS windows subsystem. This prevents a console window from opening with the
// application.
#![cfg_attr(windows, windows_subsystem = "windows")]

/// cc is short for Clone Capture, a shorthand way to clone a bunch of values before an expression
/// (particularly useful for closures).
///
/// It is defined here to allow it to be used in all submodules (textual scope lookup).
macro_rules! cc {
    ( ($($c:ident),*) $e:expr ) => {
        {
            $(let $c = $c.clone();)*
            $e
        }
    }
}

mod data;
mod ui;

use ui::*;

fn main() {
    todo!();
}
