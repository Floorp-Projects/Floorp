// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::os::raw::c_char;

use ffi_support::FfiStr;

use crate::ffi_string_ext::FallibleToString;
use crate::{define_metric, handlemap_ext::HandleMapExtension, with_glean_value, Lifetime};

define_metric!(JweMetric => JWE_METRICS {
    new           -> glean_new_jwe_metric(),
    test_get_num_recorded_errors -> glean_jwe_test_get_num_recorded_errors,
    destroy       -> glean_destroy_jwe_metric,
});

#[no_mangle]
pub extern "C" fn glean_jwe_set_with_compact_representation(metric_id: u64, value: FfiStr) {
    with_glean_value(|glean| {
        JWE_METRICS.call_with_log(metric_id, |metric| {
            let value = value.to_string_fallible()?;
            metric.set_with_compact_representation(glean, value);
            Ok(())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_jwe_set(
    metric_id: u64,
    header: FfiStr,
    key: FfiStr,
    init_vector: FfiStr,
    cipher_text: FfiStr,
    auth_tag: FfiStr,
) {
    with_glean_value(|glean| {
        JWE_METRICS.call_with_log(metric_id, |metric| {
            let header = header.to_string_fallible()?;
            let key = key.to_string_fallible()?;
            let init_vector = init_vector.to_string_fallible()?;
            let cipher_text = cipher_text.to_string_fallible()?;
            let auth_tag = auth_tag.to_string_fallible()?;
            metric.set(glean, header, key, init_vector, cipher_text, auth_tag);
            Ok(())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_jwe_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        JWE_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value(glean, storage_name.as_str())
                .is_some()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_jwe_test_get_value(metric_id: u64, storage_name: FfiStr) -> *mut c_char {
    with_glean_value(|glean| {
        JWE_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value(glean, storage_name.as_str()).unwrap()
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_jwe_test_get_value_as_json_string(
    metric_id: u64,
    storage_name: FfiStr,
) -> *mut c_char {
    with_glean_value(|glean| {
        JWE_METRICS.call_infallible(metric_id, |metric| {
            metric
                .test_get_value_as_json_string(glean, storage_name.as_str())
                .unwrap()
        })
    })
}
