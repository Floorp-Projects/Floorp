// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use {nsstring::nsACString, uuid::Uuid};

#[macro_use]
mod macros;
mod event;

#[no_mangle]
pub unsafe extern "C" fn fog_counter_add(id: u32, amount: i32) {
    let metric = metric_get!(COUNTER_MAP, id);
    metric.add(amount);
}

#[no_mangle]
pub unsafe extern "C" fn fog_counter_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(COUNTER_MAP, id, storage_name)
}

#[no_mangle]
pub unsafe extern "C" fn fog_counter_test_get_value(id: u32, storage_name: &nsACString) -> i32 {
    test_get!(COUNTER_MAP, id, storage_name)
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_start(id: u32) {
    let metric = metric_get!(TIMESPAN_MAP, id);
    metric.start();
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_stop(id: u32) {
    let metric = metric_get!(TIMESPAN_MAP, id);
    metric.stop();
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(TIMESPAN_MAP, id, storage_name)
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_test_get_value(id: u32, storage_name: &nsACString) -> u64 {
    test_get!(TIMESPAN_MAP, id, storage_name)
}

#[no_mangle]
pub unsafe extern "C" fn fog_boolean_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(BOOLEAN_MAP, id, storage_name)
}

#[no_mangle]
pub unsafe extern "C" fn fog_boolean_test_get_value(id: u32, storage_name: &nsACString) -> bool {
    test_get!(BOOLEAN_MAP, id, storage_name)
}

#[no_mangle]
pub extern "C" fn fog_boolean_set(id: u32, value: bool) {
    let metric = metric_get!(BOOLEAN_MAP, id);
    metric.set(value);
}

// The String functions are custom because test_get needs to use an outparam.
// If we can make test_get optional, we can go back to using the macro to
// generate the rest of the functions, or something.

#[no_mangle]
pub extern "C" fn fog_string_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(STRING_MAP, id, storage_name)
}

#[no_mangle]
pub extern "C" fn fog_string_test_get_value(
    id: u32,
    storage_name: &nsACString,
    value: &mut nsACString,
) {
    let val = test_get!(STRING_MAP, id, storage_name);
    value.assign(&val);
}

#[no_mangle]
pub extern "C" fn fog_string_set(id: u32, value: &nsACString) {
    let metric = metric_get!(STRING_MAP, id);
    metric.set(value.to_utf8());
}

// The Uuid functions are custom because test_get needs to use an outparam.
// If we can make test_get optional, we can go back to using the macro to
// generate the rest of the functions, or something.

#[no_mangle]
pub extern "C" fn fog_uuid_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(UUID_MAP, id, storage_name)
}

#[no_mangle]
pub extern "C" fn fog_uuid_test_get_value(
    id: u32,
    storage_name: &nsACString,
    value: &mut nsACString,
) {
    let uuid = test_get!(UUID_MAP, id, storage_name).to_string();
    value.assign(&uuid);
}

#[no_mangle]
pub extern "C" fn fog_uuid_set(id: u32, value: &nsACString) {
    if let Ok(uuid) = Uuid::parse_str(&value.to_utf8()) {
        let metric = metric_get!(UUID_MAP, id);
        metric.set(uuid);
    }
}

#[no_mangle]
pub extern "C" fn fog_uuid_generate_and_set(id: u32) {
    let metric = metric_get!(UUID_MAP, id);
    metric.generate_and_set();
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_has_value(id: u32, storage_name: &nsACString) -> bool {
    test_has!(DATETIME_MAP, id, storage_name)
}

#[no_mangle]
pub extern "C" fn fog_datetime_test_get_value(
    id: u32,
    storage_name: &nsACString,
    value: &mut nsACString,
) {
    let val = test_get!(DATETIME_MAP, id, storage_name);
    value.assign(&val);
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
    let metric = metric_get!(DATETIME_MAP, id);
    metric.set_with_details(year, month, day, hour, minute, second, nano, offset_seconds);
}
