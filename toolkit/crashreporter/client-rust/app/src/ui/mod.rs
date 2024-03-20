/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The UI model, UI implementations, and functions using them.
//!
//! UIs must implement:
//! * a `fn run_loop(&self, app: model::Application)` method which should display the UI and block while
//!   handling events until the application terminates,
//! * a `fn invoke(&self, f: model::InvokeFn)` method which invokes the given function
//!   asynchronously (without blocking) on the UI loop thread.

mod model;

#[cfg(test)]
pub mod test {
    pub mod model {
        pub use crate::ui::model::*;
    }
}
