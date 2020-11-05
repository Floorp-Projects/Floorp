/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::*;

extern "C" {
    pub fn SECOID_FindOIDByTag(tagnum: u32 /* SECOidTag */) -> *mut SECOidData;
    pub fn SECOID_DestroyAlgorithmID(aid: *mut SECAlgorithmID, freeit: PRBool);
}
