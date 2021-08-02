// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::FfiStr;

use crate::{define_metric, handlemap_ext::HandleMapExtension, with_glean_value, Lifetime};

define_metric!(CounterMetric => COUNTER_METRICS {
    new           -> glean_new_counter_metric(),
    test_get_num_recorded_errors -> glean_counter_test_get_num_recorded_errors,
    destroy       -> glean_destroy_counter_metric,

    add -> glean_counter_add(amount: i32),
});

#[no_mangle]
pub extern "C" fn glean_counter_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        COUNTER_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_counter_test_get_value(metric_id: u64, storage_name: FfiStr) -> i32 {
    with_glean_value(|glean| {
        COUNTER_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value(glean, storage_name.as_str()).unwrap()
        })
    })
}
