/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::rc::Rc;
use windows_sys::Win32::UI::WindowsAndMessaging::PostQuitMessage;

/// A Cloneable token which will post a quit message (with code 0) to the main loop when the last
/// instance is dropped.
#[derive(Clone, Default)]
pub struct QuitToken(#[allow(dead_code)] Rc<QuitTokenInternal>);

impl QuitToken {
    pub fn new() -> Self {
        Self::default()
    }
}

impl std::fmt::Debug for QuitToken {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct(std::any::type_name::<Self>())
            .finish_non_exhaustive()
    }
}

#[derive(Default)]
struct QuitTokenInternal;

impl Drop for QuitTokenInternal {
    fn drop(&mut self) {
        unsafe { PostQuitMessage(0) };
    }
}
