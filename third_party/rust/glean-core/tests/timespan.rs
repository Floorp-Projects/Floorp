// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::time::Duration;

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{test_get_num_recorded_errors, ErrorType};
use glean_core::{CommonMetricData, Lifetime};

// Tests ported from glean-ac

#[test]
fn serializer_should_correctly_serialize_timespans() {
    let (mut tempdir, _) = tempdir();

    let duration = 60;

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = TimespanMetric::new(
            CommonMetricData {
                name: "timespan_metric".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                lifetime: Lifetime::Ping,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        );

        metric.set_start(&glean, 0);
        metric.set_stop(&glean, duration);

        let val = metric
            .get_value(&glean, "store1")
            .expect("Value should be stored");
        assert_eq!(duration, val, "Recorded timespan should be positive.");
    }

    // Make a new Glean instance here, which should force reloading of the data from disk
    // so we can ensure it persisted, because it has User lifetime
    {
        let (glean, _) = new_glean(Some(tempdir));
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();

        assert_eq!(
            json!({"timespan": {"telemetry.timespan_metric": { "value": duration, "time_unit": "nanosecond" }}}),
            snapshot
        );
    }
}

#[test]
fn single_elapsed_time_must_be_recorded() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let duration = 60;

    metric.set_start(&glean, 0);
    metric.set_stop(&glean, duration);

    let val = metric
        .get_value(&glean, "store1")
        .expect("Value should be stored");
    assert_eq!(duration, val, "Recorded timespan should be positive.");
}

// SKIPPED from glean-ac: multiple elapsed times must be correctly accumulated.
// replaced by below after API change.

#[test]
fn second_timer_run_is_skipped() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let duration = 60;
    metric.set_start(&glean, 0);
    metric.set_stop(&glean, duration);

    // No error should be recorded here: we had no prior value stored.
    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState).is_err());

    let first_value = metric.get_value(&glean, "store1").unwrap();
    assert_eq!(duration, first_value);

    metric.set_start(&glean, 0);
    metric.set_stop(&glean, duration * 2);

    let second_value = metric.get_value(&glean, "store1").unwrap();
    assert_eq!(second_value, first_value);

    // Make sure that the error has been recorded: we had a stored value, the
    // new measurement was dropped.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState)
    );
}

#[test]
fn recorded_time_conforms_to_resolution() {
    let (glean, _t) = new_glean(None);

    let ns_metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_ns".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let minute_metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_m".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Minute,
    );

    let duration = 60;
    ns_metric.set_start(&glean, 0);
    ns_metric.set_stop(&glean, duration);

    let ns_value = ns_metric.get_value(&glean, "store1").unwrap();
    assert_eq!(duration, ns_value);

    // 1 minute in nanoseconds
    let duration_minute = 60 * 1_000_000_000;
    minute_metric.set_start(&glean, 0);
    minute_metric.set_stop(&glean, duration_minute);

    let minute_value = minute_metric.get_value(&glean, "store1").unwrap();
    assert_eq!(1, minute_value);
}

// SKIPPED from glean-ac: accumulated short-lived timespans should not be discarded

#[test]
fn cancel_does_not_store() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    metric.set_start(&glean, 0);
    metric.cancel();

    assert_eq!(None, metric.get_value(&glean, "store1"));
}

#[test]
fn nothing_stored_before_stop() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let duration = 60;

    metric.set_start(&glean, 0);

    assert_eq!(None, metric.get_value(&glean, "store1"));

    metric.set_stop(&glean, duration);
    assert_eq!(duration, metric.get_value(&glean, "store1").unwrap());
}

#[test]
fn set_raw_time() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let time = Duration::from_secs(1);
    metric.set_raw_sync(&glean, time);

    let time_in_ns = time.as_nanos() as u64;
    assert_eq!(Some(time_in_ns), metric.get_value(&glean, "store1"));
}

#[test]
fn set_raw_time_does_nothing_when_timer_running() {
    let (glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let time = Duration::from_secs(42);

    metric.set_start(&glean, 0);
    metric.set_raw_sync(&glean, time);
    metric.set_stop(&glean, 60);

    // We expect the start/stop value, not the raw value.
    assert_eq!(Some(60), metric.get_value(&glean, "store1"));

    // Make sure that the error has been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState)
    );
}

#[test]
fn timespan_is_not_tracked_across_upload_toggle() {
    let (mut glean, _t) = new_glean(None);

    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    // Timer is started.
    metric.set_start(&glean, 0);
    // User disables telemetry upload.
    glean.set_upload_enabled(false);
    // App code eventually stops the timer.
    // We should clear internal state as upload is disabled.
    metric.set_stop(&glean, 40);

    // App code eventually starts the timer again.
    // Upload is disabled, so this should not have any effect.
    metric.set_start(&glean, 100);
    // User enables telemetry upload again.
    glean.set_upload_enabled(true);
    // App code eventually stops the timer.
    // None should be running.
    metric.set_stop(&glean, 200);

    // Nothing should have been recorded.
    assert_eq!(None, metric.get_value(&glean, "store1"));

    // Make sure that the error has been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidState)
    );
}

#[test]
fn time_cannot_go_backwards() {
    let (glean, _t) = new_glean(None);

    let metric: TimespanMetric = TimespanMetric::new(
        CommonMetricData {
            name: "raw_timespan".into(),
            category: "test".into(),
            send_in_pings: vec!["test1".into()],
            ..Default::default()
        },
        TimeUnit::Millisecond,
    );

    // Time cannot go backwards.
    metric.set_start(&glean, 10);
    metric.set_stop(&glean, 0);
    assert!(metric.get_value(&glean, "test1").is_none());
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue),
    );
}
