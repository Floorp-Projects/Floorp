// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::Arc;

use crate::error_recording::{record_error, test_get_num_recorded_errors, ErrorType};
use crate::metrics::Metric;
use crate::metrics::MetricType;
use crate::storage::StorageManager;
use crate::CommonMetricData;
use crate::Glean;

// The maximum number of characters a URL Metric may have, before encoding.
const MAX_URL_LENGTH: usize = 2048;

/// A URL metric.
///
/// Record an Unicode string value a URL content.
/// The URL is length-limited to `MAX_URL_LENGTH` bytes.
#[derive(Clone, Debug)]
pub struct UrlMetric {
    meta: Arc<CommonMetricData>,
}

impl MetricType for UrlMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl UrlMetric {
    /// Creates a new string metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self {
            meta: Arc::new(meta),
        }
    }

    fn is_valid_url_scheme(&self, value: String) -> bool {
        let mut splits = value.split(':');
        if let Some(scheme) = splits.next() {
            if scheme.is_empty() {
                return false;
            }
            let mut chars = scheme.chars();
            // The list of characters allowed in the scheme is on
            // the spec here: https://url.spec.whatwg.org/#url-scheme-string
            return chars.next().unwrap().is_ascii_alphabetic()
                && chars.all(|c| c.is_ascii_alphanumeric() || ['+', '-', '.'].contains(&c));
        }

        // No ':' found, this is not valid :)
        false
    }

    /// Sets to the specified stringified URL.
    ///
    /// # Arguments
    ///
    /// * `value` - The stringified URL to set the metric to.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_URL_LENGTH` bytes and logs an error.
    pub fn set<S: Into<String>>(&self, value: S) {
        let value = value.into();
        let metric = self.clone();
        crate::launch_with_glean(move |glean| metric.set_sync(glean, value))
    }

    /// Sets to the specified stringified URL synchronously.
    #[doc(hidden)]
    pub fn set_sync<S: Into<String>>(&self, glean: &Glean, value: S) {
        if !self.should_record(glean) {
            return;
        }

        let s = value.into();
        if s.len() > MAX_URL_LENGTH {
            let msg = format!(
                "Value length {} exceeds maximum of {}",
                s.len(),
                MAX_URL_LENGTH
            );
            record_error(glean, &self.meta, ErrorType::InvalidOverflow, msg, None);
            return;
        }

        if s.starts_with("data:") {
            record_error(
                glean,
                &self.meta,
                ErrorType::InvalidValue,
                "URL metric does not support data URLs.",
                None,
            );
            return;
        }

        if !self.is_valid_url_scheme(s.clone()) {
            let msg = format!("\"{}\" does not start with a valid URL scheme.", s);
            record_error(glean, &self.meta, ErrorType::InvalidValue, msg, None);
            return;
        }

        let value = Metric::Url(s);
        glean.storage().record(glean, &self.meta, &value)
    }

    #[doc(hidden)]
    pub(crate) fn get_value<'a, S: Into<Option<&'a str>>>(
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
            Some(Metric::Url(s)) => Some(s),
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
    use crate::ErrorType;
    use crate::Lifetime;

    #[test]
    fn payload_is_correct() {
        let (glean, _) = new_glean(None);

        let metric = UrlMetric::new(CommonMetricData {
            name: "url_metric".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            lifetime: Lifetime::Application,
            disabled: false,
            dynamic_label: None,
        });

        let sample_url = "glean://test".to_string();
        metric.set_sync(&glean, sample_url.clone());
        assert_eq!(sample_url, metric.get_value(&glean, "store1").unwrap());
    }

    #[test]
    fn does_not_record_url_exceeding_maximum_length() {
        let (glean, _) = new_glean(None);

        let metric = UrlMetric::new(CommonMetricData {
            name: "url_metric".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            lifetime: Lifetime::Application,
            disabled: false,
            dynamic_label: None,
        });

        let long_path = "testing".repeat(2000);
        let test_url = format!("glean://{}", long_path);
        metric.set_sync(&glean, test_url);

        assert!(metric.get_value(&glean, "store1").is_none());

        assert_eq!(
            1,
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
                .unwrap()
        );
    }

    #[test]
    fn does_not_record_data_urls() {
        let (glean, _) = new_glean(None);

        let metric = UrlMetric::new(CommonMetricData {
            name: "url_metric".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            lifetime: Lifetime::Application,
            disabled: false,
            dynamic_label: None,
        });

        let test_url = "data:application/json";
        metric.set_sync(&glean, test_url);

        assert!(metric.get_value(&glean, "store1").is_none());

        assert_eq!(
            1,
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).unwrap()
        );
    }

    #[test]
    fn url_validation_works_and_records_errors() {
        let (glean, _) = new_glean(None);

        let metric = UrlMetric::new(CommonMetricData {
            name: "url_metric".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            lifetime: Lifetime::Application,
            disabled: false,
            dynamic_label: None,
        });

        let incorrects = vec![
            "",
            // Scheme may only start with upper or lowercase ASCII alpha[^1] character.
            // [1]: https://infra.spec.whatwg.org/#ascii-alpha
            "1glean://test",
            "-glean://test",
            // Scheme may only have ASCII alphanumeric characters or the `-`, `.`, `+` characters.
            "шеллы://test",
            "g!lean://test",
            "g=lean://test",
            // Scheme must be followed by `:` character.
            "glean//test",
        ];

        let corrects = vec![
            // The minimum URL
            "g:",
            // Empty body is fine
            "glean://",
            // "//" is actually not even necessary
            "glean:",
            "glean:test",
            "glean:test.com",
            // Scheme may only have ASCII alphanumeric characters or the `-`, `.`, `+` characters.
            "g-lean://test",
            "g+lean://test",
            "g.lean://test",
            // Query parameters are fine
            "glean://test?hello=world",
            // Finally, some actual real world URLs
            "https://infra.spec.whatwg.org/#ascii-alpha",
            "https://infra.spec.whatwg.org/#ascii-alpha?test=for-glean",
        ];

        for incorrect in incorrects.clone().into_iter() {
            metric.set_sync(&glean, incorrect);
            assert!(metric.get_value(&glean, "store1").is_none());
        }

        assert_eq!(
            incorrects.len(),
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).unwrap()
                as usize
        );

        for correct in corrects.into_iter() {
            metric.set_sync(&glean, correct);
            assert_eq!(metric.get_value(&glean, "store1").unwrap(), correct);
        }
    }
}
