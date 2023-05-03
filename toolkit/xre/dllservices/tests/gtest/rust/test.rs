/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

extern crate uuid;
use uuid::Uuid;

#[no_mangle]
pub extern "C" fn Rust_TriggerGenRandomFromHashMap() -> () {
    let _: HashMap<u32, u32> = HashMap::new();
}

#[no_mangle]
pub extern "C" fn Rust_TriggerGenRandomFromUuid() -> () {
    let _: Uuid = Uuid::new_v4();
}
