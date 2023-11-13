// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{test_get_num_recorded_errors, ErrorType};
use glean_core::{CommonMetricData, Lifetime};

// Tests ported from glean-ac

// SKIPPED from glean-ac: counter deserializer should correctly parse integers
// This test doesn't really apply to rkv

#[test]
fn counter_serializer_should_correctly_serialize_counters() {
    let (mut tempdir, _) = tempdir();

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = CounterMetric::new(CommonMetricData {
            name: "counter_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });

        metric.add_sync(&glean, 1);

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"counter": {"telemetry.counter_metric": 1}}),
            snapshot
        );
    }

    // Make a new Glean instance here, which should force reloading of the data from disk
    // so we can ensure it persisted, because it has User lifetime
    {
        let (glean, _t) = new_glean(Some(tempdir));
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"counter": {"telemetry.counter_metric": 1}}),
            snapshot
        );
    }
}

#[test]
fn set_value_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = CounterMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.add_sync(&glean, 1);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!({"counter": {"telemetry.counter_metric": 1}}),
            snapshot
        );
    }
}

// SKIPPED from glean-ac: counters are serialized in the correct JSON format
// Completely redundant with other tests.

#[test]
fn counters_must_not_increment_when_passed_zero_or_negative() {
    let (glean, _t) = new_glean(None);

    let metric = CounterMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Application,
        ..Default::default()
    });

    // Attempt to increment the counter with zero
    metric.add_sync(&glean, 0);
    // Check that nothing was recorded
    assert!(metric.get_value(&glean, Some("store1")).is_none());

    // Attempt to increment the counter with negative
    metric.add_sync(&glean, -1);
    // Check that nothing was recorded
    assert!(metric.get_value(&glean, Some("store1")).is_none());

    // Attempt increment counter properly
    metric.add_sync(&glean, 1);
    // Check that nothing was recorded
    assert_eq!(1, metric.get_value(&glean, Some("store1")).unwrap());

    // Make sure that the error has been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );
}

// New tests for glean-core below

#[test]
fn transformation_works() {
    let (glean, _t) = new_glean(None);

    let counter: CounterMetric = CounterMetric::new(CommonMetricData {
        name: "transformation".into(),
        category: "local".into(),
        send_in_pings: vec!["store1".into(), "store2".into()],
        ..Default::default()
    });

    counter.add_sync(&glean, 2);

    assert_eq!(2, counter.get_value(&glean, Some("store1")).unwrap());
    assert_eq!(2, counter.get_value(&glean, Some("store2")).unwrap());

    // Clearing just one store
    let _ = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    counter.add_sync(&glean, 2);

    assert_eq!(2, counter.get_value(&glean, Some("store1")).unwrap());
    assert_eq!(4, counter.get_value(&glean, Some("store2")).unwrap());
}

#[test]
fn saturates_at_boundary() {
    let (glean, _t) = new_glean(None);

    let counter: CounterMetric = CounterMetric::new(CommonMetricData {
        name: "transformation".into(),
        category: "local".into(),
        send_in_pings: vec!["store1".into()],
        ..Default::default()
    });

    counter.add_sync(&glean, 2);
    counter.add_sync(&glean, i32::max_value());

    assert_eq!(
        i32::max_value(),
        counter.get_value(&glean, Some("store1")).unwrap()
    );
}
