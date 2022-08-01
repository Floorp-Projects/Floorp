// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use crate::error_recording::{test_get_num_recorded_errors, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::util::truncate_string_at_boundary_with_error;
use crate::CommonMetricData;
use crate::Glean;

const MAX_LENGTH_VALUE: usize = 100;

/// A string metric.
///
/// Record an Unicode string value with arbitrary content.
/// Strings are length-limited to `MAX_LENGTH_VALUE` bytes.
#[derive(Clone, Debug)]
pub struct StringMetric {
    meta: Arc<CommonMetricData>,
}

impl MetricType for StringMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn with_name(&self, name: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.name = name;
        Self {
            meta: Arc::new(meta),
        }
    }

    fn with_dynamic_label(&self, label: String) -> Self {
        let mut meta = (*self.meta).clone();
        meta.dynamic_label = Some(label);
        Self {
            meta: Arc::new(meta),
        }
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl StringMetric {
    /// Creates a new string metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            meta: Arc::new(meta),
        }
    }

    /// Sets to the specified value.
    ///
    /// # Arguments
    ///
    /// * `value` - The string to set the metric to.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_LENGTH_VALUE` bytes and logs an error.
    pub fn set(&self, value: String) {
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, &value))
    }

    /// Sets to the specified value synchronously.
    #[doc(hidden)]
    pub fn set_sync<S: Into<String>>(&self, glean: &Glean, value: S) {
        if !self.should_record(glean) {
            return;
        }

        let s = truncate_string_at_boundary_with_error(glean, &self.meta, value, MAX_LENGTH_VALUE);

        let value = Metric::String(s);
        glean.storage().record(glean, &self.meta, &value)
    }

    /// Gets the current-stored value as a string, or None if there is no value.
    #[doc(hidden)]
    pub fn get_value<'a, S: Into<Option<&'a str>>>(
        &self,
        glean: &Glean,
        ping_name: S,
    ) -> Option<String> {
        let queried_ping_name = ping_name
            .into()
            .unwrap_or_else(|| &self.meta().send_in_pings[0]);

        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            queried_ping_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::String(s)) => Some(s),
            _ => None,
        }
    }

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, ping_name: Option<String>) -> Option<String> {
        crate::block_on_dispatcher();
        crate::core::with_glean(|glean| self.get_value(glean, ping_name.as_deref()))
    }

    /// **Exported for test purposes.**
    ///
    /// Gets the number of recorded errors for the given metric and error type.
    ///
    /// # Arguments
    ///
    /// * `error` - The type of error
    /// * `ping_name` - represents the optional name of the ping to retrieve the
    ///   metric for. Defaults to the first value in `send_in_pings`.
    ///
    /// # Returns
    ///
    /// The number of errors reported.
    pub fn test_get_num_recorded_errors(&self, error: ErrorType) -> i32 {
        crate::block_on_dispatcher();

        crate::core::with_glean(|glean| {
            test_get_num_recorded_errors(glean, self.meta(), error).unwrap_or(0)
        })
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::test_get_num_recorded_errors;
    use crate::tests::new_glean;
    use crate::util::truncate_string_at_boundary;
    use crate::ErrorType;
    use crate::Lifetime;

    #[test]
    fn setting_a_long_string_records_an_error() {
        let (glean, _) = new_glean(None);

        let metric = StringMetric::new(CommonMetricData {
            name: "string_metric".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            lifetime: Lifetime::Application,
            disabled: false,
            dynamic_label: None,
        });

        let sample_string = "0123456789".repeat(11);
        metric.set_sync(&glean, sample_string.clone());

        let truncated = truncate_string_at_boundary(sample_string, MAX_LENGTH_VALUE);
        assert_eq!(truncated, metric.get_value(&glean, "store1").unwrap());

        assert_eq!(
            1,
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
                .unwrap()
        );
    }
}
