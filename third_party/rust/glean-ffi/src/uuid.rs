// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::os::raw::c_char;

use ffi_support::FfiStr;

use crate::{
    define_metric, ffi_string_ext::FallibleToString, handlemap_ext::HandleMapExtension,
    with_glean_value, Lifetime,
};

define_metric!(UuidMetric => UUID_METRICS {
    new           -> glean_new_uuid_metric(),
    test_get_num_recorded_errors -> glean_uuid_test_get_num_recorded_errors,
    destroy       -> glean_destroy_uuid_metric,
});

#[no_mangle]
pub extern "C" fn glean_uuid_set(metric_id: u64, value: FfiStr) {
    with_glean_value(|glean| {
        UUID_METRICS.call_with_log(metric_id, |metric| {
            let value = value.to_string_fallible()?;
            metric.set_from_str(glean, &value);
            Ok(())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_uuid_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        UUID_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_uuid_test_get_value(metric_id: u64, storage_name: FfiStr) -> *mut c_char {
    with_glean_value(|glean| {
        UUID_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .unwrap()
                .to_string()
        })
    })
}
