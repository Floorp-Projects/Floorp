/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::{c_int, c_uint};

pub type PRIntn = c_int;
pub type PRBool = PRIntn;
pub type PRUword = usize;
pub type PRInt32 = c_int;
pub type PRUint32 = c_uint;
pub const PR_FALSE: PRBool = 0;
pub const PR_TRUE: PRBool = 1;
