// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use ffi_support::FfiStr;

use crate::{define_metric, handlemap_ext::HandleMapExtension, with_glean_value, Lifetime};

define_metric!(QuantityMetric => QUANTITY_METRICS {
    new           -> glean_new_quantity_metric(),
    test_get_num_recorded_errors -> glean_quantity_test_get_num_recorded_errors,
    destroy       -> glean_destroy_quantity_metric,

    set -> glean_quantity_set(value: i64),
});

#[no_mangle]
pub extern "C" fn glean_quantity_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        QUANTITY_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_quantity_test_get_value(metric_id: u64, storage_name: FfiStr) -> i64 {
    with_glean_value(|glean| {
        QUANTITY_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value(glean, storage_name.as_str()).unwrap()
        })
    })
}
