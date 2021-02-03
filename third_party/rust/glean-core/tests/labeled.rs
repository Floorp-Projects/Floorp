// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{CommonMetricData, Lifetime};

#[test]
fn can_create_labeled_counter_metric() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        Some(vec!["label1".into()]),
    );

    let metric = labeled.get("label1");
    metric.add(&glean, 1);

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "telemetry.labeled_metric": { "label1": 1 }
            }
        }),
        snapshot
    );
}

#[test]
fn can_create_labeled_string_metric() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        StringMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        Some(vec!["label1".into()]),
    );

    let metric = labeled.get("label1");
    metric.set(&glean, "text");

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_string": {
                "telemetry.labeled_metric": { "label1": "text" }
            }
        }),
        snapshot
    );
}

#[test]
fn can_create_labeled_bool_metric() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        BooleanMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        Some(vec!["label1".into()]),
    );

    let metric = labeled.get("label1");
    metric.set(&glean, true);

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_boolean": {
                "telemetry.labeled_metric": { "label1": true }
            }
        }),
        snapshot
    );
}

#[test]
fn can_use_multiple_labels() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    let metric = labeled.get("label1");
    metric.add(&glean, 1);

    let metric = labeled.get("label2");
    metric.add(&glean, 2);

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "telemetry.labeled_metric": {
                    "label1": 1,
                    "label2": 2,
                }
            }
        }),
        snapshot
    );
}

#[test]
fn labels_are_checked_against_static_list() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        Some(vec!["label1".into(), "label2".into()]),
    );

    let metric = labeled.get("label1");
    metric.add(&glean, 1);

    let metric = labeled.get("label2");
    metric.add(&glean, 2);

    // All non-registed labels get mapped to the `other` label
    let metric = labeled.get("label3");
    metric.add(&glean, 3);
    let metric = labeled.get("label4");
    metric.add(&glean, 4);

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "telemetry.labeled_metric": {
                    "label1": 1,
                    "label2": 2,
                    "__other__": 7,
                }
            }
        }),
        snapshot
    );
}

#[test]
fn dynamic_labels_too_long() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    let metric = labeled.get("this_string_has_more_than_thirty_characters");
    metric.add(&glean, 1);

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "glean.error.invalid_label": { "telemetry.labeled_metric": 1 },
                "telemetry.labeled_metric": {
                    "__other__": 1,
                }
            }
        }),
        snapshot
    );
}

#[test]
fn dynamic_labels_regex_mismatch() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    let labels_not_validating = vec![
        "notSnakeCase",
        "",
        "with/slash",
        "1.not_fine",
        "this.$isnotfine",
        "-.not_fine",
        "this.is_not_fine.2",
    ];
    let num_non_validating = labels_not_validating.len();

    for label in &labels_not_validating {
        labeled.get(label).add(&glean, 1);
    }

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "glean.error.invalid_label": { "telemetry.labeled_metric": num_non_validating },
                "telemetry.labeled_metric": {
                    "__other__": num_non_validating,
                }
            }
        }),
        snapshot
    );
}

#[test]
fn dynamic_labels_regex_allowed() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    let labels_validating = vec![
        "this.is.fine",
        "this_is_fine_too",
        "this.is_still_fine",
        "thisisfine",
        "_.is_fine",
        "this.is-fine",
        "this-is-fine",
    ];

    for label in &labels_validating {
        labeled.get(label).add(&glean, 1);
    }

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    assert_eq!(
        json!({
            "labeled_counter": {
                "telemetry.labeled_metric": {
                    "this.is.fine": 1,
                    "this_is_fine_too": 1,
                    "this.is_still_fine": 1,
                    "thisisfine": 1,
                    "_.is_fine": 1,
                    "this.is-fine": 1,
                    "this-is-fine": 1
                }
            }
        }),
        snapshot
    );
}

#[test]
fn seen_labels_get_reloaded_from_disk() {
    let (mut tempdir, _) = tempdir();

    let (glean, dir) = new_glean(Some(tempdir));
    tempdir = dir;

    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "labeled_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    // Store some data into labeled metrics
    {
        // Set the maximum number of labels
        for i in 1..=16 {
            let label = format!("label{}", i);
            labeled.get(&label).add(&glean, i);
        }

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", false)
            .unwrap();

        // Check that the data is there
        for i in 1..=16 {
            let label = format!("label{}", i);
            assert_eq!(
                i,
                snapshot["labeled_counter"]["telemetry.labeled_metric"][&label]
            );
        }

        drop(glean);
    }

    // Force a reload
    {
        let (glean, _) = new_glean(Some(tempdir));

        // Try to store another label
        labeled.get("new_label").add(&glean, 40);

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", false)
            .unwrap();

        // Check that the old data is still there
        for i in 1..=16 {
            let label = format!("label{}", i);
            assert_eq!(
                i,
                snapshot["labeled_counter"]["telemetry.labeled_metric"][&label]
            );
        }

        // The new label lands in the __other__ bucket, due to too many labels
        assert_eq!(
            40,
            snapshot["labeled_counter"]["telemetry.labeled_metric"]["__other__"]
        );
    }
}

#[test]
fn caching_metrics_with_dynamic_labels() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "cached_labels".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    // Create multiple metric instances and cache them for later use.
    let metrics = (1..=20)
        .map(|i| {
            let label = format!("label{}", i);
            labeled.get(&label)
        })
        .collect::<Vec<_>>();

    // Only now use them.
    for metric in metrics {
        metric.add(&glean, 1);
    }

    // The maximum number of labels we store is 16.
    // So we should have put 4 metrics in the __other__ bucket.
    let other = labeled.get("__other__");
    assert_eq!(Some(4), other.test_get_value(&glean, "store1"));
}

#[test]
fn caching_metrics_with_dynamic_labels_across_pings() {
    let (glean, _t) = new_glean(None);
    let labeled = LabeledMetric::new(
        CounterMetric::new(CommonMetricData {
            name: "cached_labels2".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        }),
        None,
    );

    // Create multiple metric instances and cache them for later use.
    let metrics = (1..=20)
        .map(|i| {
            let label = format!("label{}", i);
            labeled.get(&label)
        })
        .collect::<Vec<_>>();

    // Only now use them.
    for metric in &metrics {
        metric.add(&glean, 1);
    }

    // The maximum number of labels we store is 16.
    // So we should have put 4 metrics in the __other__ bucket.
    let other = labeled.get("__other__");
    assert_eq!(Some(4), other.test_get_value(&glean, "store1"));

    // Snapshot (so we can inspect the JSON)
    // and clear out storage (the same way submitting a ping would)
    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    // We didn't send the 20th label
    assert_eq!(
        json!(null),
        snapshot["labeled_counter"]["telemetry.cached_labels2"]["label20"]
    );

    // We now set the ones that ended up in `__other__` before.
    // Note: indexing is zero-based,
    // but we later check the names, so let's offset it by 1.
    metrics[16].add(&glean, 17);
    metrics[17].add(&glean, 18);
    metrics[18].add(&glean, 19);
    metrics[19].add(&glean, 20);

    assert_eq!(Some(17), metrics[16].test_get_value(&glean, "store1"));
    assert_eq!(Some(18), metrics[17].test_get_value(&glean, "store1"));
    assert_eq!(Some(19), metrics[18].test_get_value(&glean, "store1"));
    assert_eq!(Some(20), metrics[19].test_get_value(&glean, "store1"));
    assert_eq!(None, other.test_get_value(&glean, "store1"));

    let snapshot = StorageManager
        .snapshot_as_json(glean.storage(), "store1", true)
        .unwrap();

    let cached_labels = &snapshot["labeled_counter"]["telemetry.cached_labels2"];
    assert_eq!(json!(17), cached_labels["label17"]);
    assert_eq!(json!(18), cached_labels["label18"]);
    assert_eq!(json!(19), cached_labels["label19"]);
    assert_eq!(json!(20), cached_labels["label20"]);
    assert_eq!(json!(null), cached_labels["__other__"]);
}
