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
fn object_serializer_should_correctly_serialize_objects() {
    let (mut tempdir, _) = tempdir();

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = ObjectMetric::new(CommonMetricData {
            name: "object_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });

        let obj = serde_json::from_str("{ \"value\": 1 }").unwrap();
        metric.set_sync(&glean, obj);

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"object": {"telemetry.object_metric": { "value": 1 }}}),
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
            json!({"object": {"telemetry.object_metric": { "value": 1 }}}),
            snapshot
        );
    }
}

#[test]
fn set_value_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = ObjectMetric::new(CommonMetricData {
        name: "object_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    let obj = serde_json::from_str("{ \"value\": 1 }").unwrap();
    metric.set_sync(&glean, obj);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!({"object": {"telemetry.object_metric": { "value": 1 }}}),
            snapshot
        );
    }
}

#[test]
fn getting_data_json_encoded() {
    let (glean, _t) = new_glean(None);

    let object: ObjectMetric = ObjectMetric::new(CommonMetricData {
        name: "transformation".into(),
        category: "local".into(),
        send_in_pings: vec!["store1".into()],
        ..Default::default()
    });

    let obj_str = "{\"value\":1}";
    let obj = serde_json::from_str(obj_str).unwrap();
    object.set_sync(&glean, obj);

    assert_eq!(obj_str, object.get_value(&glean, Some("store1")).unwrap());
}
