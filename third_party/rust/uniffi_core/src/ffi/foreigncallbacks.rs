/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains code to handle foreign callbacks - C-ABI functions that are defined by a
//! foreign language, then registered with UniFFI.  These callbacks are used to implement callback
//! interfaces, async scheduling etc. Foreign callbacks are registered at startup, when the foreign
//! code loads the exported library. For each callback type, we also define a "cell" type for
//! storing the callback.

use std::{
    ptr::{null_mut, NonNull},
    sync::atomic::{AtomicPtr, Ordering},
};

// Cell type that stores any NonNull<T>
#[doc(hidden)]
pub struct UniffiForeignPointerCell<T>(AtomicPtr<T>);

impl<T> UniffiForeignPointerCell<T> {
    pub const fn new() -> Self {
        Self(AtomicPtr::new(null_mut()))
    }

    pub fn set(&self, callback: NonNull<T>) {
        self.0.store(callback.as_ptr(), Ordering::Relaxed);
    }

    pub fn get(&self) -> &T {
        unsafe {
            NonNull::new(self.0.load(Ordering::Relaxed))
                .expect("Foreign pointer not set.  This is likely a uniffi bug.")
                .as_mut()
        }
    }
}

unsafe impl<T> Send for UniffiForeignPointerCell<T> {}
unsafe impl<T> Sync for UniffiForeignPointerCell<T> {}
