// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::convert::TryFrom;
use std::os::raw::c_char;

use ffi_support::FfiStr;

use glean_core::metrics::EventMetric;
use glean_core::{CommonMetricData, Lifetime};

use crate::ffi_string_ext::FallibleToString;
use crate::handlemap_ext::HandleMapExtension;
use crate::{
    define_metric, from_raw_int_array_and_string_array, from_raw_string_array, with_glean_value,
    RawIntArray, RawStringArray,
};

define_metric!(EventMetric => EVENT_METRICS {
    test_get_num_recorded_errors -> glean_event_test_get_num_recorded_errors,
    destroy       -> glean_destroy_event_metric,
});

#[no_mangle]
pub extern "C" fn glean_new_event_metric(
    category: FfiStr,
    name: FfiStr,
    send_in_pings: RawStringArray,
    send_in_pings_len: i32,
    lifetime: i32,
    disabled: u8,
    extra_keys: RawStringArray,
    extra_keys_len: i32,
) -> u64 {
    EVENT_METRICS.insert_with_log(|| {
        let name = name.to_string_fallible()?;
        let category = category.to_string_fallible()?;
        let send_in_pings = from_raw_string_array(send_in_pings, send_in_pings_len)?;
        let lifetime = Lifetime::try_from(lifetime)?;
        let extra_keys = from_raw_string_array(extra_keys, extra_keys_len)?;

        Ok(EventMetric::new(
            CommonMetricData {
                name,
                category,
                send_in_pings,
                lifetime,
                disabled: disabled != 0,
                ..Default::default()
            },
            extra_keys,
        ))
    })
}

#[no_mangle]
pub extern "C" fn glean_event_record(
    metric_id: u64,
    timestamp: u64,
    extra_keys: RawIntArray,
    extra_values: RawStringArray,
    extra_len: i32,
) {
    with_glean_value(|glean| {
        EVENT_METRICS.call_with_log(metric_id, |metric| {
            let extra = from_raw_int_array_and_string_array(extra_keys, extra_values, extra_len)?;
            metric.record(glean, timestamp, extra);
            Ok(())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_event_test_has_value(metric_id: u64, storage_name: FfiStr) -> u8 {
    with_glean_value(|glean| {
        EVENT_METRICS.call_infallible(metric_id, |metric| {
            metric.test_has_value(glean, storage_name.as_str())
        })
    })
}

#[no_mangle]
pub extern "C" fn glean_event_test_get_value_as_json_string(
    metric_id: u64,
    storage_name: FfiStr,
) -> *mut c_char {
    with_glean_value(|glean| {
        EVENT_METRICS.call_infallible(metric_id, |metric| {
            metric.test_get_value_as_json_string(glean, storage_name.as_str())
        })
    })
}
