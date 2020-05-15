/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::*;

extern "C" {
    pub fn SECKEY_CopyPublicKey(pubKey: *const SECKEYPublicKey) -> *mut SECKEYPublicKey;
    pub fn SECKEY_ConvertToPublicKey(privateKey: *mut SECKEYPrivateKey) -> *mut SECKEYPublicKey;
    pub fn SECKEY_DestroyPrivateKey(key: *mut SECKEYPrivateKey);
    pub fn SECKEY_DestroyPublicKey(key: *mut SECKEYPublicKey);
}
