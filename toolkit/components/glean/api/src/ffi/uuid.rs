// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;
use uuid::Uuid;

#[no_mangle]
pub extern "C" fn fog_uuid_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(UUID_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_uuid_test_get_value(id: u32, ping_name: &nsACString, value: &mut nsACString) {
    let uuid = with_metric!(UUID_MAP, id, metric, test_get!(metric, ping_name)).to_string();
    value.assign(&uuid);
}

#[no_mangle]
pub extern "C" fn fog_uuid_set(id: u32, value: &nsACString) {
    if let Ok(uuid) = Uuid::parse_str(&value.to_utf8()) {
        with_metric!(UUID_MAP, id, metric, metric.set(uuid));
    }
}

#[no_mangle]
pub extern "C" fn fog_uuid_generate_and_set(id: u32) {
    with_metric!(UUID_MAP, id, metric, metric.generate_and_set());
}

#[no_mangle]
pub extern "C" fn fog_uuid_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(UUID_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
