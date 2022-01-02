// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::time::Duration;

use ffi_support::FfiStr;

use crate::{define_metric, handlemap_ext::HandleMapExtension, with_glean_value, Lifetime};

define_metric!(TimespanMetric => TIMESPAN_METRICS {
    new           -> glean_new_timespan_metric(time_unit: i32),
    test_get_num_recorded_errors -> glean_timespan_test_get_num_recorded_errors,
    destroy       -> glean_destroy_timespan_metric,
});

#[no_mangle]
pub extern "C" fn glean_timespan_set_start(metric_id: u64, start_time: u64) {
    with_glean_value(|glean| {
        TIMESPAN_METRICS.call_infallible_mut(metric_id, |metric| {
            metric.set_start(glean, start_time);
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_timespan_set_stop(metric_id: u64, stop_time: u64) {
    with_glean_value(|glean| {
        TIMESPAN_METRICS.call_infallible_mut(metric_id, |metric| {
            metric.set_stop(glean, stop_time);
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_timespan_cancel(metric_id: u64) {
    TIMESPAN_METRICS.call_infallible_mut(metric_id, |metric| {
        metric.cancel();
    })
}

#[no_mangle]
pub extern "C" fn glean_timespan_set_raw_nanos(metric_id: u64, elapsed_nanos: u64) {
    let elapsed = Duration::from_nanos(elapsed_nanos);
    with_glean_value(|glean| {
        TIMESPAN_METRICS.call_infallible(metric_id, |metric| {
            metric.set_raw(glean, elapsed);
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_timespan_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        TIMESPAN_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_timespan_test_get_value(metric_id: u64, storage_name: FfiStr) -> u64 {
    with_glean_value(|glean| {
        TIMESPAN_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value(glean, storage_name.as_str()).unwrap()
        })
    })
}
