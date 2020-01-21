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
fn uuid_is_generated_and_stored() {
    let (mut glean, _t) = new_glean(None);

    let uuid: UuidMetric = UuidMetric::new(CommonMetricData {
        name: "uuid".into(),
        category: "local".into(),
        send_in_pings: vec!["core".into()],
        ..Default::default()
    });

    uuid.generate_and_set(&glean);
    let snapshot = glean.snapshot("core", false);
    assert!(
        snapshot.contains(r#""local.uuid": ""#),
        format!("Snapshot 1: {}", snapshot)
    );

    uuid.generate_and_set(&glean);
    let snapshot = glean.snapshot("core", false);
    assert!(
        snapshot.contains(r#""local.uuid": ""#),
        format!("Snapshot 2: {}", snapshot)
    );
}

#[test]
fn uuid_serializer_should_correctly_serialize_uuids() {
    let value = uuid::Uuid::new_v4();

    let (mut tempdir, _) = tempdir();

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = UuidMetric::new(CommonMetricData {
            name: "uuid_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });

        metric.set(&glean, value);

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"uuid": {"telemetry.uuid_metric": value.to_string()}}),
            snapshot
        );
    }

    // Make a new Glean instance here, which should force reloading of the data from disk
    // so we can ensure it persisted, because it has User lifetime
    {
        let (glean, _) = new_glean(Some(tempdir));
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"uuid": {"telemetry.uuid_metric": value.to_string()}}),
            snapshot
        );
    }
}

#[test]
fn set_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];
    let value = uuid::Uuid::new_v4();

    let metric = UuidMetric::new(CommonMetricData {
        name: "uuid_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set(&glean, value);

    // Check that the data was correctly set in each store.
    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!({"uuid": {"telemetry.uuid_metric": value.to_string()}}),
            snapshot
        );
    }
}
