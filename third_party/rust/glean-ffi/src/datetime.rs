// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::os::raw::c_char;

use ffi_support::FfiStr;

use crate::{
    define_metric, handlemap_ext::HandleMapExtension, with_glean_value, Lifetime, TimeUnit,
};

define_metric!(DatetimeMetric => DATETIME_METRICS {
    new           -> glean_new_datetime_metric(time_unit: TimeUnit),
    test_get_num_recorded_errors -> glean_datetime_test_get_num_recorded_errors,
    destroy       -> glean_destroy_datetime_metric,
});

#[no_mangle]
pub extern "C" fn glean_datetime_set(
    metric_id: u64,
    year: i32,
    month: u32,
    day: u32,
    hour: u32,
    minute: u32,
    second: u32,
    nano: i64,
    offset_seconds: i32,
) {
    // Convert and truncate the nanos to u32, as that's what the underlying
    // library uses. Unfortunately, not all platform have unsigned integers
    // so we need to work with what we have.
    if nano < 0 || nano > i64::from(std::u32::MAX) {
        log::error!("Unexpected `nano` value coming from platform code {}", nano);
        return;
    }

    // We are within the u32 boundaries for nano, we should be ok converting.
    let converted_nanos = nano as u32;
    with_glean_value(|glean| {
        DATETIME_METRICS.call_infallible(metric_id, |metric| {
            metric.set_with_details(
                glean,
                year,
                month,
                day,
                hour,
                minute,
                second,
                converted_nanos,
                offset_seconds,
            );
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_datetime_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        DATETIME_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value_as_string(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_datetime_test_get_value_as_string(
    metric_id: u64,
    storage_name: FfiStr,
) -> *mut c_char {
    with_glean_value(|glean| {
        DATETIME_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value_as_string(glean, storage_name.as_str())
                .unwrap()
        })
    })
}
