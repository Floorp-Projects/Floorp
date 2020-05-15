/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::*;
use std::os::raw::c_char;

extern "C" {
    pub fn PR_GetError() -> PRErrorCode;
    pub fn PR_GetErrorTextLength() -> PRInt32;
    pub fn PR_GetErrorText(text: *mut c_char) -> PRInt32;
}

pub type PRErrorCode = PRInt32;
