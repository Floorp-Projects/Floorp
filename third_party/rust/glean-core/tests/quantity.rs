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

// SKIPPED from glean-ac: quantity deserializer should correctly parse integers
// This test doesn't really apply to rkv

#[test]
fn quantity_serializer_should_correctly_serialize_quantities() {
    let (mut tempdir, _) = tempdir();

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = QuantityMetric::new(CommonMetricData {
            name: "quantity_metric".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });

        metric.set_sync(&glean, 1);

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"quantity": {"telemetry.quantity_metric": 1}}),
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
            json!({"quantity": {"telemetry.quantity_metric": 1}}),
            snapshot
        );
    }
}

#[test]
fn set_value_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = QuantityMetric::new(CommonMetricData {
        name: "quantity_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_sync(&glean, 1);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!({"quantity": {"telemetry.quantity_metric": 1}}),
            snapshot
        );
    }
}

// SKIPPED from glean-ac: quantities are serialized in the correct JSON format
// Completely redundant with other tests.

#[test]
fn quantities_must_not_set_when_passed_negative() {
    let (glean, _t) = new_glean(None);

    let metric = QuantityMetric::new(CommonMetricData {
        name: "quantity_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Application,
        ..Default::default()
    });

    // Attempt to set the quantity with negative
    metric.set_sync(&glean, -1);
    // Check that nothing was recorded
    assert!(metric.get_value(&glean, "store1").is_none());

    // Make sure that the errors have been recorded
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );
}
