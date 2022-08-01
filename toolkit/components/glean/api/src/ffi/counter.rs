// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

#[no_mangle]
pub unsafe extern "C" fn fog_counter_add(id: u32, amount: i32) {
    with_metric!(COUNTER_MAP, id, metric, metric.add(amount));
}

#[no_mangle]
pub unsafe extern "C" fn fog_counter_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(COUNTER_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub unsafe extern "C" fn fog_counter_test_get_value(id: u32, ping_name: &nsACString) -> i32 {
    with_metric!(COUNTER_MAP, id, metric, test_get!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_counter_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(COUNTER_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
