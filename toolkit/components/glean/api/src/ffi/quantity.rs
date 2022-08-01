// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

#[no_mangle]
pub extern "C" fn fog_quantity_set(id: u32, value: i64) {
    with_metric!(QUANTITY_MAP, id, metric, metric.set(value));
}

#[no_mangle]
pub extern "C" fn fog_quantity_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(QUANTITY_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_quantity_test_get_value(id: u32, ping_name: &nsACString) -> i64 {
    with_metric!(QUANTITY_MAP, id, metric, test_get!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_quantity_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(QUANTITY_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
