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
fn snapshot_returns_none_if_nothing_is_recorded_in_the_store() {
    let (glean, _t) = new_glean(None);
    assert!(StorageManager
        .snapshot(glean.storage(), "unknown_store", true)
        .is_none())
}

#[test]
fn can_snapshot() {
    let (glean, _t) = new_glean(None);

    let local_metric = StringMetric::new(CommonMetricData {
        name: "can_snapshot_local_metric".into(),
        category: "local".into(),
        send_in_pings: vec!["store".into()],
        ..Default::default()
    });

    local_metric.set_sync(&glean, "snapshot 42");

    assert!(StorageManager
        .snapshot(glean.storage(), "store", true)
        .is_some())
}

#[test]
fn snapshot_correctly_clears_the_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = CounterMetric::new(CommonMetricData {
        name: "metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names,
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.add_sync(&glean, 1);

    // Get the snapshot from "store1" and clear it.
    let snapshot = StorageManager.snapshot(glean.storage(), "store1", true);
    assert!(snapshot.is_some());
    // Check that getting a new snapshot for "store1" returns an empty store.
    assert!(StorageManager
        .snapshot(glean.storage(), "store1", false)
        .is_none());
    // Check that we get the right data from both the stores. Clearing "store1" must
    // not clear "store2" as well.
    let snapshot2 = StorageManager.snapshot(glean.storage(), "store2", true);
    assert!(snapshot2.is_some());
}

#[test]
fn storage_is_thread_safe() {
    use std::sync::{Arc, Barrier, Mutex};
    use std::thread;

    let (glean, _t) = new_glean(None);
    let glean = Arc::new(Mutex::new(glean));

    let threadsafe_metric = CounterMetric::new(CommonMetricData {
        name: "threadsafe".into(),
        category: "global".into(),
        send_in_pings: vec!["core".into(), "metrics".into()],
        ..Default::default()
    });
    let threadsafe_metric = Arc::new(threadsafe_metric);

    let barrier = Arc::new(Barrier::new(2));
    let c = barrier.clone();
    let threadsafe_metric_clone = threadsafe_metric.clone();
    let glean_clone = glean.clone();
    let child = thread::spawn(move || {
        threadsafe_metric_clone.add_sync(&glean_clone.lock().unwrap(), 1);
        c.wait();
        threadsafe_metric_clone.add_sync(&glean_clone.lock().unwrap(), 1);
    });

    threadsafe_metric.add_sync(&glean.lock().unwrap(), 1);
    barrier.wait();
    threadsafe_metric.add_sync(&glean.lock().unwrap(), 1);

    child.join().unwrap();

    let snapshot = StorageManager
        .snapshot_as_json(glean.lock().unwrap().storage(), "core", true)
        .unwrap();
    assert_eq!(json!({"counter": { "global.threadsafe": 4 }}), snapshot);
}
