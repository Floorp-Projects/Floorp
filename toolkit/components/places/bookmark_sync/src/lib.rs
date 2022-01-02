/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

#[macro_use]
extern crate cstr;
#[macro_use]
extern crate xpcom;

mod driver;
mod error;
mod merger;
mod store;

use xpcom::{interfaces::mozISyncedBookmarksMerger, RefPtr};

use crate::merger::SyncedBookmarksMerger;

#[no_mangle]
pub unsafe extern "C" fn NS_NewSyncedBookmarksMerger(
    result: *mut *const mozISyncedBookmarksMerger,
) {
    let merger = SyncedBookmarksMerger::new();
    RefPtr::new(merger.coerce::<mozISyncedBookmarksMerger>()).forget(&mut *result);
}
