// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

#[no_mangle]
pub extern "C" fn fog_datetime_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(DATETIME_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut nsACString,
) {
    let val = with_metric!(DATETIME_MAP, id, metric, test_get!(metric, ping_name));
    value.assign(&val.to_rfc3339());
}

#[no_mangle]
pub extern "C" fn fog_datetime_set(
    id: u32,
    year: i32,
    month: u32,
    day: u32,
    hour: u32,
    minute: u32,
    second: u32,
    nano: u32,
    offset_seconds: i32,
) {
    with_metric!(
        DATETIME_MAP,
        id,
        metric,
        metric.set_with_details(year, month, day, hour, minute, second, nano, offset_seconds)
    );
}
