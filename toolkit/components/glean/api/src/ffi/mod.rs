// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#![cfg(feature = "with_gecko")]

use crate::pings;
use nsstring::{nsACString, nsCString};
use std::sync::atomic::Ordering;
use thin_vec::ThinVec;
use uuid::Uuid;

#[macro_use]
mod macros;
mod event;

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
pub unsafe extern "C" fn fog_timespan_start(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.start());
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_stop(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.stop());
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_cancel(id: u32) {
    with_metric!(TIMESPAN_MAP, id, metric, metric.cancel());
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_set_raw(id: u32, duration: u32) {
    with_metric!(
        TIMESPAN_MAP,
        id,
        metric,
        metric.set_raw_unitless(duration.into())
    );
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(TIMESPAN_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub unsafe extern "C" fn fog_timespan_test_get_value(id: u32, ping_name: &nsACString) -> u64 {
    with_metric!(TIMESPAN_MAP, id, metric, test_get!(metric, ping_name))
}

#[no_mangle]
pub unsafe extern "C" fn fog_boolean_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(BOOLEAN_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub unsafe extern "C" fn fog_boolean_test_get_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(BOOLEAN_MAP, id, metric, test_get!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_boolean_set(id: u32, value: bool) {
    with_metric!(BOOLEAN_MAP, id, metric, metric.set(value));
}

// The String functions are custom because test_get needs to use an outparam.
// If we can make test_get optional, we can go back to using the macro to
// generate the rest of the functions, or something.

#[no_mangle]
pub extern "C" fn fog_string_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(STRING_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_string_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut nsACString,
) {
    let val = with_metric!(STRING_MAP, id, metric, test_get!(metric, ping_name));
    value.assign(&val);
}

#[no_mangle]
pub extern "C" fn fog_string_set(id: u32, value: &nsACString) {
    with_metric!(STRING_MAP, id, metric, metric.set(value.to_utf8()));
}

// String List Functions:

#[no_mangle]
pub extern "C" fn fog_string_list_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(STRING_LIST_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub extern "C" fn fog_string_list_test_get_value(
    id: u32,
    ping_name: &nsACString,
    value: &mut ThinVec<nsCString>,
) {
    let val = with_metric!(STRING_LIST_MAP, id, metric, test_get!(metric, ping_name));
    for v in val {
        value.push(v.into());
    }
}

#[no_mangle]
pub extern "C" fn fog_string_list_add(id: u32, value: &nsACString) {
    with_metric!(STRING_LIST_MAP, id, metric, metric.add(value.to_utf8()));
}

#[no_mangle]
pub extern "C" fn fog_string_list_set(id: u32, value: &ThinVec<nsCString>) {
    let value = value.iter().map(|s| s.to_utf8().into()).collect();
    with_metric!(STRING_LIST_MAP, id, metric, metric.set(value));
}

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

#[no_mangle]
pub extern "C" fn fog_memory_distribution_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        test_has!(metric, ping_name)
    )
}

#[no_mangle]
pub extern "C" fn fog_memory_distribution_test_get_value(
    id: u32,
    ping_name: &nsACString,
    sum: &mut u64,
    buckets: &mut ThinVec<u64>,
    counts: &mut ThinVec<u64>,
) {
    let val = with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        test_get!(metric, ping_name)
    );
    *sum = val.sum;
    for (&bucket, &count) in val.values.iter() {
        buckets.push(bucket);
        counts.push(count);
    }
}

#[no_mangle]
pub extern "C" fn fog_memory_distribution_accumulate(id: u32, sample: u64) {
    with_metric!(
        MEMORY_DISTRIBUTION_MAP,
        id,
        metric,
        metric.accumulate(sample)
    );
}

#[no_mangle]
pub extern "C" fn fog_submit_ping_by_id(id: u32, reason: &nsACString) {
    let reason = if reason.is_empty() {
        None
    } else {
        Some(reason.to_utf8())
    };
    pings::submit_ping_by_id(id, reason.as_deref());
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_start(id: u32) -> u64 {
    with_metric!(TIMING_DISTRIBUTION_MAP, id, metric, metric.start())
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_stop_and_accumulate(id: u32, timing_id: u64) {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        metric.stop_and_accumulate(timing_id)
    );
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_cancel(id: u32, timing_id: u64) {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        metric.cancel(timing_id)
    );
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        test_has!(metric, ping_name)
    )
}

#[no_mangle]
pub extern "C" fn fog_timing_distribution_test_get_value(
    id: u32,
    ping_name: &nsACString,
    sum: &mut u64,
    buckets: &mut ThinVec<u64>,
    counts: &mut ThinVec<u64>,
) {
    let val = with_metric!(
        TIMING_DISTRIBUTION_MAP,
        id,
        metric,
        test_get!(metric, ping_name)
    );
    *sum = val.sum;
    for (&bucket, &count) in val.values.iter() {
        buckets.push(bucket);
        counts.push(count);
    }
}

#[no_mangle]
pub extern "C" fn fog_labeled_boolean_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_BOOLEAN_MAP,
        BOOLEAN_MAP,
        LabeledBooleanMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_counter_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_COUNTER_MAP,
        COUNTER_MAP,
        LabeledCounterMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_labeled_string_get(id: u32, label: &nsACString) -> u32 {
    labeled_submetric_get!(
        id,
        label,
        LABELED_STRING_MAP,
        STRING_MAP,
        LabeledStringMetric
    )
}

#[no_mangle]
pub extern "C" fn fog_quantity_set(id: u32, value: i64) {
    with_metric!(QUANTITY_MAP, id, metric, metric.set(value));
}

#[no_mangle]
pub unsafe extern "C" fn fog_quantity_test_has_value(id: u32, ping_name: &nsACString) -> bool {
    with_metric!(QUANTITY_MAP, id, metric, test_has!(metric, ping_name))
}

#[no_mangle]
pub unsafe extern "C" fn fog_quantity_test_get_value(id: u32, ping_name: &nsACString) -> i64 {
    with_metric!(QUANTITY_MAP, id, metric, test_get!(metric, ping_name))
}
