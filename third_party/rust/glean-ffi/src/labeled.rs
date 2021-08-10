// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::convert::TryFrom;

use glean_core::{metrics::*, CommonMetricData, Lifetime};

use crate::boolean::BOOLEAN_METRICS;
use crate::counter::COUNTER_METRICS;
use crate::string::STRING_METRICS;
use crate::*;

/// Generate FFI functions for labeled metrics.
///
/// This can be used to reduce the amount of duplicated boilerplate around calling
/// `LabeledMetric::new` and LabeledMetric.get`.
/// The constructor function takes the general common meta data.
///
/// If any additional data needs to be passed in, this macro cannot be used.
///
/// Arguments:
///
/// * `metric` - The metric type, e.g. `CounterMetric`.
/// * `global` - The name of the newly constructed global to hold instances of the labeled metric.
/// * `metric_global` - The name of the map to hold instances of the underlying metric type.
/// * `new_name` - Function name to create a new labeled metric of this type.
/// * `destroy_name` - Function name to destroy the labeled metric.
/// * `get_name` - Function name to get a new instance of the underlying metric.
macro_rules! impl_labeled_metric {
    ($metric:ty, $global:ident, $metric_global:ident, $new_name:ident, $destroy_name:ident, $get_name:ident, $test_get_num_recorded_errors:ident) => {
        static $global: once_cell::sync::Lazy<ConcurrentHandleMap<LabeledMetric<$metric>>> =
            once_cell::sync::Lazy::new(ConcurrentHandleMap::new);
        $crate::define_infallible_handle_map_deleter!($global, $destroy_name);

        /// Create a new labeled metric.
        #[no_mangle]
        pub extern "C" fn $new_name(
            category: FfiStr,
            name: FfiStr,
            send_in_pings: RawStringArray,
            send_in_pings_len: i32,
            lifetime: i32,
            disabled: u8,
            labels: RawStringArray,
            label_count: i32,
        ) -> u64 {
            $global.insert_with_log(|| {
                let name = name.to_string_fallible()?;
                let category = category.to_string_fallible()?;
                let send_in_pings = from_raw_string_array(send_in_pings, send_in_pings_len)?;
                let labels = from_raw_string_array(labels, label_count)?;
                let labels = if labels.is_empty() {
                    None
                } else {
                    Some(labels)
                };
                let lifetime = Lifetime::try_from(lifetime)?;

                Ok(LabeledMetric::new(
                    <$metric>::new(CommonMetricData {
                        name,
                        category,
                        send_in_pings,
                        lifetime,
                        disabled: disabled != 0,
                        ..Default::default()
                    }),
                    labels,
                ))
            })
        }

        /// Create a new instance of the sub-metric of this labeled metric.
        #[no_mangle]
        pub extern "C" fn $get_name(handle: u64, label: FfiStr) -> u64 {
            $global.call_infallible_mut(handle, |labeled| {
                let metric = labeled.get(label.as_str());
                $metric_global.insert_with_log(|| Ok(metric))
            })
        }

        #[no_mangle]
        pub extern "C" fn $test_get_num_recorded_errors(
            metric_id: u64,
            error_type: i32,
            storage_name: FfiStr,
        ) -> i32 {
            crate::with_glean_value(|glean| {
                crate::HandleMapExtension::call_infallible(&*$global, metric_id, |metric| {
                    let error_type = std::convert::TryFrom::try_from(error_type).unwrap();
                    let storage_name =
                        crate::FallibleToString::to_string_fallible(&storage_name).unwrap();
                    glean_core::test_get_num_recorded_errors(
                        glean,
                        &metric.get_submetric().meta(),
                        error_type,
                        Some(&storage_name),
                    )
                    .unwrap_or(0)
                })
            })
        }
    };
}

// Create the required FFI functions for LabeledMetric<CounterMetric>
impl_labeled_metric!(
    CounterMetric,
    LABELED_COUNTER,
    COUNTER_METRICS,
    glean_new_labeled_counter_metric,
    glean_destroy_labeled_counter_metric,
    glean_labeled_counter_metric_get,
    glean_labeled_counter_test_get_num_recorded_errors
);

// Create the required FFI functions for LabeledMetric<BooleanMetric>
impl_labeled_metric!(
    BooleanMetric,
    LABELED_BOOLEAN,
    BOOLEAN_METRICS,
    glean_new_labeled_boolean_metric,
    glean_destroy_labeled_boolean_metric,
    glean_labeled_boolean_metric_get,
    glean_labeled_boolean_test_get_num_recorded_errors
);

// Create the required FFI functions for LabeledMetric<StringMetric>
impl_labeled_metric!(
    StringMetric,
    LABELED_STRING,
    STRING_METRICS,
    glean_new_labeled_string_metric,
    glean_destroy_labeled_string_metric,
    glean_labeled_string_metric_get,
    glean_labeled_string_test_get_num_recorded_errors
);
