// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::os::raw::c_char;

use ffi_support::FfiStr;

use crate::{
    define_metric, ffi_string_ext::FallibleToString, handlemap_ext::HandleMapExtension, GLEAN,
};

define_metric!(StringMetric => STRING_METRICS {
    new           -> glean_new_string_metric(),
    test_get_num_recorded_errors -> glean_string_test_get_num_recorded_errors,
    destroy       -> glean_destroy_string_metric,
});

#[no_mangle]
pub extern "C" fn glean_string_set(glean_handle: u64, metric_id: u64, value: FfiStr) {
    GLEAN.call_infallible(glean_handle, |glean| {
        STRING_METRICS.call_with_log(metric_id, |metric| {
            let value = value.to_string_fallible()?;
            metric.set(glean, value);
            Ok(())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_string_test_has_value(
    glean_handle: u64,
    metric_id: u64,
    storage_name: FfiStr,
) -> u8 {
    GLEAN.call_infallible(glean_handle, |glean| {
        STRING_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_string_test_get_value(
    glean_handle: u64,
    metric_id: u64,
    storage_name: FfiStr,
) -> *mut c_char {
    GLEAN.call_infallible(glean_handle, |glean| {
        STRING_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value(glean, storage_name.as_str()).unwrap()
        })
    })
}
