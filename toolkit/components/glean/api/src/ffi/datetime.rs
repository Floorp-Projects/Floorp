// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use nsstring::nsACString;

#[repr(C)]
pub struct FogDatetime {
    year: i32,
    month: u32,
    day: u32,
    hour: u32,
    minute: u32,
    second: u32,
    nano: u32,
    offset_seconds: i32,
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(DATETIME_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut FogDatetime,
) {
    let val = with_metric!(DATETIME_MAP, id, metric, test_get!(metric, ping_name));
    value.year = val.year;
    value.month = val.month;
    value.day = val.day;
    value.hour = val.hour;
    value.minute = val.minute;
    value.second = val.second;
    value.nano = val.nanosecond;
    value.offset_seconds = val.offset_seconds;
}

#[no_mangle]
pub extern "C" fn fog_datetime_set(id: u32, dt: &FogDatetime) {
    with_metric!(
        DATETIME_MAP,
        id,
        metric,
        metric.set_with_details(
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second,
            dt.nano,
            dt.offset_seconds
        )
    );
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_get_error(id: u32, error_str: &mut nsACString) -> bool {
    let err = with_metric!(DATETIME_MAP, id, metric, test_get_errors!(metric));
    err.map(|err_str| error_str.assign(&err_str)).is_some()
}
