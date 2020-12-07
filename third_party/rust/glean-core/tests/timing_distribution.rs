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

        let mut metric = TimingDistributionMetric::new(
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

        let id = metric.set_start(0);
        metric.set_stop_and_accumulate(&glean, id, duration);

        let snapshot = metric
            .test_get_value(&glean, "store1")
            .expect("Value should be stored");

        assert_eq!(snapshot.sum, duration);
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

    let mut metric = TimingDistributionMetric::new(
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

    let id = metric.set_start(0);
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

    let mut metric = TimingDistributionMetric::new(
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
    let id = metric.set_start(duration);
    metric.set_stop_and_accumulate(&glean, id, 0);

    assert!(metric.test_get_value(&glean, "store1").is_none());

    // Make sure that the errors have been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(
            &glean,
            metric.meta(),
            ErrorType::InvalidValue,
            Some("store1")
        )
    );
}

#[test]
fn the_accumulate_samples_api_correctly_stores_timing_values() {
    let (glean, _t) = new_glean(None);

    let mut metric = TimingDistributionMetric::new(
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
    metric.accumulate_samples_signed(&glean, [1, 2, 3].to_vec());

    let snapshot = metric
        .test_get_value(&glean, "store1")
        .expect("Value should be stored");

    let seconds_to_nanos = 1000 * 1000 * 1000;

    // Check that we got the right sum and number of samples.
    assert_eq!(snapshot.sum, 6 * seconds_to_nanos);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * seconds_to_nanos)` for `i = 1..=3`.
    assert_eq!(1, snapshot.values[&984_625_593]);
    assert_eq!(1, snapshot.values[&1_969_251_187]);
    assert_eq!(1, snapshot.values[&2_784_941_737]);

    // No errors should be reported.
    assert!(test_get_num_recorded_errors(
        &glean,
        metric.meta(),
        ErrorType::InvalidValue,
        Some("store1")
    )
    .is_err());
}

#[test]
fn the_accumulate_samples_api_correctly_handles_negative_values() {
    let (glean, _t) = new_glean(None);

    let mut metric = TimingDistributionMetric::new(
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
    metric.accumulate_samples_signed(&glean, [-1, 1, 2, 3].to_vec());

    let snapshot = metric
        .test_get_value(&glean, "store1")
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
        test_get_num_recorded_errors(
            &glean,
            metric.meta(),
            ErrorType::InvalidValue,
            Some("store1")
        )
    );
}

#[test]
fn the_accumulate_samples_api_correctly_handles_overflowing_values() {
    let (glean, _t) = new_glean(None);

    let mut metric = TimingDistributionMetric::new(
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
    metric.accumulate_samples_signed(&glean, [overflowing_val, 1, 2, 3].to_vec());

    let snapshot = metric
        .test_get_value(&glean, "store1")
        .expect("Value should be stored");

    // Overflowing values are truncated to MAX_SAMPLE_TIME and recorded.
    assert_eq!(snapshot.sum, MAX_SAMPLE_TIME + 6);

    // We should get a sample in each of the first 3 buckets.
    assert_eq!(1, snapshot.values[&1]);
    assert_eq!(1, snapshot.values[&2]);
    assert_eq!(1, snapshot.values[&3]);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(
            &glean,
            metric.meta(),
            ErrorType::InvalidOverflow,
            Some("store1")
        )
    );
}

#[test]
fn large_nanoseconds_values() {
    let (glean, _t) = new_glean(None);

    let mut metric = TimingDistributionMetric::new(
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

    let id = metric.set_start(0);
    metric.set_stop_and_accumulate(&glean, id, time);

    let val = metric
        .test_get_value(&glean, "store1")
        .expect("Value should be stored");

    // Check that we got the right sum and number of samples.
    assert_eq!(val.sum, time);
}

#[test]
fn stopping_non_existing_id_records_an_error() {
    let (glean, _t) = new_glean(None);

    let mut metric = TimingDistributionMetric::new(
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

    metric.set_stop_and_accumulate(&glean, 3785, 60);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(
            &glean,
            metric.meta(),
            ErrorType::InvalidState,
            Some("store1")
        )
    );
}
