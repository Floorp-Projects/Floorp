// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! # Error Recording
//!
//! Glean keeps track of errors that occured due to invalid labels or invalid values when recording
//! other metrics.
//!
//! Error counts are stored in labeled counters in the `glean.error` category.
//! The labeled counter metrics that store the errors are defined in the `metrics.yaml` for documentation purposes,
//! but are not actually used directly, since the `send_in_pings` value needs to match the pings of the metric that is erroring (plus the "metrics" ping),
//! not some constant value that we could define in `metrics.yaml`.

use std::convert::TryFrom;
use std::fmt::Display;

use crate::error::{Error, ErrorKind};
use crate::metrics::CounterMetric;
use crate::metrics::{combine_base_identifier_and_label, strip_label};
use crate::CommonMetricData;
use crate::Glean;
use crate::Lifetime;

/// The possible error types for metric recording.
/// Note: the cases in this enum must be kept in sync with the ones
/// in the platform-specific code (e.g. `ErrorType.kt`) and with the
/// metrics in the registry files.
#[derive(Debug, PartialEq)]
pub enum ErrorType {
    /// For when the value to be recorded does not match the metric-specific restrictions
    InvalidValue,
    /// For when the label of a labeled metric does not match the restrictions
    InvalidLabel,
    /// For when the metric caught an invalid state while recording
    InvalidState,
    /// For when the value to be recorded overflows the metric-specific upper range
    InvalidOverflow,
}

impl ErrorType {
    /// The error type's metric id
    pub fn as_str(&self) -> &'static str {
        match self {
            ErrorType::InvalidValue => "invalid_value",
            ErrorType::InvalidLabel => "invalid_label",
            ErrorType::InvalidState => "invalid_state",
            ErrorType::InvalidOverflow => "invalid_overflow",
        }
    }
}

impl TryFrom<i32> for ErrorType {
    type Error = Error;

    fn try_from(value: i32) -> Result<ErrorType, Self::Error> {
        match value {
            0 => Ok(ErrorType::InvalidValue),
            1 => Ok(ErrorType::InvalidLabel),
            2 => Ok(ErrorType::InvalidState),
            3 => Ok(ErrorType::InvalidOverflow),
            e => Err(ErrorKind::Lifetime(e).into()),
        }
    }
}

/// For a given metric, get the metric in which to record errors
fn get_error_metric_for_metric(meta: &CommonMetricData, error: ErrorType) -> CounterMetric {
    // Can't use meta.identifier here, since that might cause infinite recursion
    // if the label on this metric needs to report an error.
    let identifier = meta.base_identifier();
    let name = strip_label(&identifier);

    // Record errors in the pings the metric is in, as well as the metrics ping.
    let mut send_in_pings = meta.send_in_pings.clone();
    let ping_name = "metrics".to_string();
    if !send_in_pings.contains(&ping_name) {
        send_in_pings.push(ping_name);
    }

    CounterMetric::new(CommonMetricData {
        name: combine_base_identifier_and_label(error.as_str(), name),
        category: "glean.error".into(),
        lifetime: Lifetime::Ping,
        send_in_pings,
        ..Default::default()
    })
}

/// Records an error into Glean.
///
/// Errors are recorded as labeled counters in the `glean.error` category.
///
/// *Note*: We do make assumptions here how labeled metrics are encoded, namely by having the name
/// `<name>/<label>`.
/// Errors do not adhere to the usual "maximum label" restriction.
///
/// # Arguments
///
/// * `glean` - The Glean instance containing the database
/// * `meta` - The metric's meta data
/// * `error` -  The error type to record
/// * `message` - The message to log. This message is not sent with the ping.
///             It does not need to include the metric id, as that is automatically prepended to the message.
/// * `num_errors` - The number of errors of the same type to report.
pub fn record_error<O: Into<Option<i32>>>(
    glean: &Glean,
    meta: &CommonMetricData,
    error: ErrorType,
    message: impl Display,
    num_errors: O,
) {
    let metric = get_error_metric_for_metric(meta, error);

    log::warn!("{}: {}", meta.base_identifier(), message);
    let to_report = num_errors.into().unwrap_or(1);
    debug_assert!(to_report > 0);
    metric.add(glean, to_report);
}

/// Gets the number of recorded errors for the given metric and error type.
///
/// *Notes: This is a **test-only** API, but we need to expose it to be used in integration tests.
///
/// # Arguments
///
/// * `glean` - The Glean object holding the database
/// * `meta` - The metadata of the metric instance
/// * `error` - The type of error
///
/// # Returns
///
/// The number of errors reported.
pub fn test_get_num_recorded_errors(
    glean: &Glean,
    meta: &CommonMetricData,
    error: ErrorType,
    ping_name: Option<&str>,
) -> Result<i32, String> {
    let use_ping_name = ping_name.unwrap_or(&meta.send_in_pings[0]);
    let metric = get_error_metric_for_metric(meta, error);

    metric.test_get_value(glean, use_ping_name).ok_or_else(|| {
        format!(
            "No error recorded for {} in '{}' store",
            meta.base_identifier(),
            use_ping_name
        )
    })
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::metrics::*;
    use crate::tests::new_glean;

    #[test]
    fn error_type_i32_mapping() {
        let error: ErrorType = std::convert::TryFrom::try_from(0).unwrap();
        assert_eq!(error, ErrorType::InvalidValue);
        let error: ErrorType = std::convert::TryFrom::try_from(1).unwrap();
        assert_eq!(error, ErrorType::InvalidLabel);
        let error: ErrorType = std::convert::TryFrom::try_from(2).unwrap();
        assert_eq!(error, ErrorType::InvalidState);
        let error: ErrorType = std::convert::TryFrom::try_from(3).unwrap();
        assert_eq!(error, ErrorType::InvalidOverflow);
    }

    #[test]
    fn recording_of_all_error_types() {
        let (glean, _t) = new_glean(None);

        let string_metric = StringMetric::new(CommonMetricData {
            name: "string_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into(), "store2".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });

        let expected_invalid_values_errors: i32 = 1;
        let expected_invalid_labels_errors: i32 = 2;

        record_error(
            &glean,
            string_metric.meta(),
            ErrorType::InvalidValue,
            "Invalid value",
            None,
        );

        record_error(
            &glean,
            string_metric.meta(),
            ErrorType::InvalidLabel,
            "Invalid label",
            expected_invalid_labels_errors,
        );

        for store in &["store1", "store2", "metrics"] {
            assert_eq!(
                Ok(expected_invalid_values_errors),
                test_get_num_recorded_errors(
                    &glean,
                    string_metric.meta(),
                    ErrorType::InvalidValue,
                    Some(store)
                )
            );
            assert_eq!(
                Ok(expected_invalid_labels_errors),
                test_get_num_recorded_errors(
                    &glean,
                    string_metric.meta(),
                    ErrorType::InvalidLabel,
                    Some(store)
                )
            );
        }
    }
}
