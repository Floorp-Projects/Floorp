/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nsstring;
extern crate uuid;
use nsstring::nsACString;
use uuid::Uuid;

use std::fmt::Write;

#[no_mangle]
pub extern "C" fn GkRustUtils_GenerateUUID(res: &mut nsACString) {
    let uuid = Uuid::new_v4();
    write!(res, "{{{}}}", uuid.to_hyphenated_ref()).expect("Unexpected uuid generated");
}
