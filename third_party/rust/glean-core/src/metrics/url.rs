// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::error_recording::{record_error, ErrorType};
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
    meta: CommonMetricData,
}

impl MetricType for UrlMetric {
    fn meta(&self) -> &CommonMetricData {
        &self.meta
    }

    fn meta_mut(&mut self) -> &mut CommonMetricData {
        &mut self.meta
    }
}

// IMPORTANT:
//
// When changing this implementation, make sure all the operations are
// also declared in the related trait in `../traits/`.
impl UrlMetric {
    /// Creates a new string metric.
    pub fn new(meta: CommonMetricData) -> Self {
        Self { meta }
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
    /// * `glean` - The Glean instance this metric belongs to.
    /// * `value` - The stringified URL to set the metric to.
    ///
    /// ## Notes
    ///
    /// Truncates the value if it is longer than `MAX_URL_LENGTH` bytes and logs an error.
    pub fn set<S: Into<String>>(&self, glean: &Glean, value: S) {
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

    /// **Test-only API (exported for FFI purposes).**
    ///
    /// Gets the currently stored value as a string.
    ///
    /// This doesn't clear the stored value.
    pub fn test_get_value(&self, glean: &Glean, storage_name: &str) -> Option<String> {
        match StorageManager.snapshot_metric_for_test(
            glean.storage(),
            storage_name,
            &self.meta.identifier(glean),
            self.meta.lifetime,
        ) {
            Some(Metric::Url(s)) => Some(s),
            _ => None,
        }
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
        metric.set(&glean, sample_url.clone());
        assert_eq!(sample_url, metric.test_get_value(&glean, "store1").unwrap());
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
        metric.set(&glean, test_url);

        assert!(metric.test_get_value(&glean, "store1").is_none());

        assert_eq!(
            1,
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow, None)
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
        metric.set(&glean, test_url);

        assert!(metric.test_get_value(&glean, "store1").is_none());

        assert_eq!(
            1,
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue, None)
                .unwrap()
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
            metric.set(&glean, incorrect);
            assert!(metric.test_get_value(&glean, "store1").is_none());
        }

        assert_eq!(
            incorrects.len(),
            test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue, None)
                .unwrap() as usize
        );

        for correct in corrects.into_iter() {
            metric.set(&glean, correct);
            assert_eq!(metric.test_get_value(&glean, "store1").unwrap(), correct);
        }
    }
}
