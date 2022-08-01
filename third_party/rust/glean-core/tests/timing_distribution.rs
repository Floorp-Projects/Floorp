// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use std::time::Duration;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{test_get_num_recorded_errors, ErrorType};
use glean_core::{CommonMetricData, Lifetime};

// Tests ported from glean-ac

#[test]
fn serializer_should_correctly_serialize_timing_distribution() {
    let (mut tempdir, _) = tempdir();

    let duration = 60;
    let time_unit = TimeUnit::Nanosecond;

    {
        let (glean, dir) = new_glean(Some(tempdir));
        tempdir = dir;

        let metric = TimingDistributionMetric::new(
            CommonMetricData {
                name: "distribution".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                lifetime: Lifetime::Ping,
                ..Default::default()
            },
            time_unit,
        );

        let id = 4u64.into();
        metric.set_start(id, 0);
        metric.set_stop_and_accumulate(&glean, id, duration);

        let snapshot = metric
            .get_value(&glean, "store1")
            .expect("Value should be stored");

        assert_eq!(snapshot.sum, duration as i64);
    }

    // Make a new Glean instance here, which should force reloading of the data from disk
    // so we can ensure it persisted, because it has User lifetime
    {
        let (glean, _) = new_glean(Some(tempdir));
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();

        assert_eq!(
            json!(duration),
            snapshot["timing_distribution"]["telemetry.distribution"]["sum"]
        );
    }
}

#[test]
fn set_value_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let duration = 1;

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let id = 4u64.into();
    metric.set_start(id, 0);
    metric.set_stop_and_accumulate(&glean, id, duration);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!(duration),
            snapshot["timing_distribution"]["telemetry.distribution"]["sum"]
        );
        assert_eq!(
            json!(1),
            snapshot["timing_distribution"]["telemetry.distribution"]["values"]["1"]
        );
    }
}

#[test]
fn timing_distributions_must_not_accumulate_negative_values() {
    let (glean, _t) = new_glean(None);

    let duration = 60;
    let time_unit = TimeUnit::Nanosecond;

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        time_unit,
    );

    // Flip around the timestamps, this should result in a negative value which should be
    // discarded.
    let id = 4u64.into();
    metric.set_start(id, duration);
    metric.set_stop_and_accumulate(&glean, id, 0);

    assert!(metric.get_value(&glean, "store1").is_none());

    // Make sure that the errors have been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );
}

#[test]
fn the_accumulate_samples_api_correctly_stores_timing_values() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Second,
    );

    // Accumulate the samples. We intentionally do not report
    // negative values to not trigger error reporting.
    metric.accumulate_samples_sync(&glean, [1, 2, 3].to_vec());

    let snapshot = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    let seconds_to_nanos = 1000 * 1000 * 1000;

    // Check that we got the right sum and number of samples.
    assert_eq!(snapshot.sum, 6 * seconds_to_nanos);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * seconds_to_nanos)` for `i = 1..=3`.
    assert_eq!(1, snapshot.values[&984625593]);
    assert_eq!(1, snapshot.values[&1969251187]);
    assert_eq!(1, snapshot.values[&2784941737]);

    // No errors should be reported.
    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).is_err());
}

#[test]
fn the_accumulate_samples_api_correctly_handles_negative_values() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    // Accumulate the samples.
    metric.accumulate_samples_sync(&glean, [-1, 1, 2, 3].to_vec());

    let snapshot = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    // Check that we got the right sum and number of samples.
    assert_eq!(snapshot.sum, 6);

    // We should get a sample in each of the first 3 buckets.
    assert_eq!(1, snapshot.values[&1]);
    assert_eq!(1, snapshot.values[&2]);
    assert_eq!(1, snapshot.values[&3]);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );
}

#[test]
fn the_accumulate_samples_api_correctly_handles_overflowing_values() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    // The MAX_SAMPLE_TIME is the same from `metrics/timing_distribution.rs`.
    const MAX_SAMPLE_TIME: u64 = 1000 * 1000 * 1000 * 60 * 10;
    let overflowing_val = MAX_SAMPLE_TIME as i64 + 1;
    // Accumulate the samples.
    metric.accumulate_samples_sync(&glean, [overflowing_val, 1, 2, 3].to_vec());

    let snapshot = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    // Overflowing values are truncated to MAX_SAMPLE_TIME and recorded.
    assert_eq!(snapshot.sum as u64, MAX_SAMPLE_TIME + 6);

    // We should get a sample in each of the first 3 buckets.
    assert_eq!(1, snapshot.values[&1]);
    assert_eq!(1, snapshot.values[&2]);
    assert_eq!(1, snapshot.values[&3]);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
    );
}

#[test]
fn large_nanoseconds_values() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let time = Duration::from_secs(10).as_nanos() as u64;
    assert!(time > u64::from(u32::max_value()));

    let id = 4u64.into();
    metric.set_start(id, 0);
    metric.set_stop_and_accumulate(&glean, id, time);

    let val = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    // Check that we got the right sum and number of samples.
    assert_eq!(val.sum, time as i64);
}

#[test]
fn stopping_non_existing_id_records_an_error() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "non_existing_id".into(),
            category: "test".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let id = 3785u64.into();
    metric.set_stop_and_accumulate(&glean, id, 60);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState)
    );
}

#[test]
fn the_accumulate_raw_samples_api_correctly_stores_timing_values() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Second,
    );

    let seconds_to_nanos = 1000 * 1000 * 1000;
    metric.accumulate_raw_samples_nanos_sync(
        &glean,
        [seconds_to_nanos, 2 * seconds_to_nanos, 3 * seconds_to_nanos].as_ref(),
    );

    let snapshot = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    // Check that we got the right sum and number of samples.
    assert_eq!(snapshot.sum, 6 * seconds_to_nanos as i64);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * seconds_to_nanos)` for `i = 1..=3`.
    assert_eq!(1, snapshot.values[&984625593]);
    assert_eq!(1, snapshot.values[&1969251187]);
    assert_eq!(1, snapshot.values[&2784941737]);

    // No errors should be reported.
    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState).is_err());
}

#[test]
fn raw_samples_api_error_cases() {
    let (glean, _t) = new_glean(None);

    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    // 10minutes in nanoseconds
    let max_sample_time = 1000 * 1000 * 1000 * 60 * 10;

    metric.accumulate_raw_samples_nanos_sync(
        &glean,
        &[
            0,                   /* rounded up to 1 */
            1,                   /* valid */
            max_sample_time + 1, /* larger then the maximum, will record an error and the maximum */
        ],
    );

    let snapshot = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");

    // Check that we got the right sum and number of samples.
    assert_eq!(snapshot.sum, 2 + max_sample_time as i64);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * seconds_to_nanos)` for `i = {1, max_sample_time}`.
    assert_eq!(2, snapshot.values[&1]);
    assert_eq!(1, snapshot.values[&599512966122]);

    // No errors should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
    );
}
