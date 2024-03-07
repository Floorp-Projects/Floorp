/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Encapsulate thread-bound values in a safe manner.
//!
//! This allows non-`Send`/`Sync` values to be transferred across thread boundaries, checking at
//! runtime at access sites whether it is safe to use them.

pub struct ThreadBound<T> {
    data: T,
    origin: std::thread::ThreadId,
}

impl<T: Default> Default for ThreadBound<T> {
    fn default() -> Self {
        ThreadBound::new(Default::default())
    }
}

impl<T> ThreadBound<T> {
    pub fn new(data: T) -> Self {
        ThreadBound {
            data,
            origin: std::thread::current().id(),
        }
    }

    pub fn borrow(&self) -> &T {
        assert!(
            std::thread::current().id() == self.origin,
            "unsafe access to thread-bound value"
        );
        &self.data
    }
}

// # Safety
// Access to the inner value is only permitted on the originating thread.
unsafe impl<T> Send for ThreadBound<T> {}
unsafe impl<T> Sync for ThreadBound<T> {}
