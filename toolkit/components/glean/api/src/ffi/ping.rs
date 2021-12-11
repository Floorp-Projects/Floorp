// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use crate::pings;
use nsstring::nsACString;

#[no_mangle]
pub extern "C" fn fog_submit_ping_by_id(id: u32, reason: &nsACString) {
    let reason = if reason.is_empty() {
        None
    } else {
        Some(reason.to_utf8())
    };
    pings::submit_ping_by_id(id, reason.as_deref());
}
