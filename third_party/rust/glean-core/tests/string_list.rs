// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{test_get_num_recorded_errors, CommonMetricData, ErrorType, Lifetime};

#[test]
fn list_can_store_multiple_items() {
    let (glean, _t) = new_glean(None);

    let list: StringListMetric = StringListMetric::new(CommonMetricData {
        name: "list".into(),
        category: "local".into(),
        send_in_pings: vec!["core".into()],
        ..Default::default()
    });

    list.add_sync(&glean, "first");
    assert_eq!(list.get_value(&glean, "core").unwrap(), vec!["first"]);

    list.add_sync(&glean, "second");
    assert_eq!(
        list.get_value(&glean, "core").unwrap(),
        vec!["first", "second"]
    );

    list.set_sync(&glean, vec!["third".into()]);
    assert_eq!(list.get_value(&glean, "core").unwrap(), vec!["third"]);

    list.add_sync(&glean, "fourth");
    assert_eq!(
        list.get_value(&glean, "core").unwrap(),
        vec!["third", "fourth"]
    );
}

#[test]
fn stringlist_serializer_should_correctly_serialize_stringlists() {
    let (mut tempdir, _) = tempdir();

    {
        // We give tempdir to the `new_glean` function...
        let (glean, dir) = new_glean(Some(tempdir));
        // And then we get it back once that function returns.
        tempdir = dir;

        let metric = StringListMetric::new(CommonMetricData {
            name: "string_list_metric".into(),
            category: "telemetry.test".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::User,
            ..Default::default()
        });
        metric.set_sync(&glean, vec!["test_string_1".into(), "test_string_2".into()]);
    }

    {
        let (glean, _) = new_glean(Some(tempdir));

        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();
        assert_eq!(
            json!({"string_list": {"telemetry.test.string_list_metric": ["test_string_1", "test_string_2"]}}),
            snapshot
        );
    }
}

#[test]
fn set_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean(None);
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry.test".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_sync(&glean, vec!["test_string_1".into(), "test_string_2".into()]);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!({"string_list": {"telemetry.test.string_list_metric": ["test_string_1", "test_string_2"]}}),
            snapshot
        );
    }
}

#[test]
fn long_string_values_are_truncated() {
    let (glean, _t) = new_glean(None);

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry.test".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    let test_string = "0123456789".repeat(20);
    metric.add_sync(&glean, test_string.clone());

    // Ensure the string was truncated to the proper length.
    assert_eq!(
        vec![test_string[..50].to_string()],
        metric.get_value(&glean, "store1").unwrap()
    );

    // Ensure the error has been recorded.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
    );

    metric.set_sync(&glean, vec![test_string.clone()]);

    // Ensure the string was truncated to the proper length.
    assert_eq!(
        vec![test_string[..50].to_string()],
        metric.get_value(&glean, "store1").unwrap()
    );

    // Ensure the error has been recorded.
    assert_eq!(
        Ok(2),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidOverflow)
    );
}

#[test]
fn disabled_string_lists_dont_record() {
    let (glean, _t) = new_glean(None);

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry.test".into(),
        send_in_pings: vec!["store1".into()],
        disabled: true,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.add_sync(&glean, "test_string".repeat(20));

    // Ensure the string was not added.
    assert_eq!(None, metric.get_value(&glean, "store1"));

    metric.set_sync(&glean, vec!["test_string_2".repeat(20)]);

    // Ensure the stringlist was not set.
    assert_eq!(None, metric.get_value(&glean, "store1"));

    // Ensure no error was recorded.
    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).is_err());
}

#[test]
fn string_lists_dont_exceed_max_items() {
    let (glean, _t) = new_glean(None);

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry.test".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    for _n in 1..21 {
        metric.add_sync(&glean, "test_string");
    }

    let expected: Vec<String> = "test_string "
        .repeat(20)
        .split_whitespace()
        .map(|s| s.to_string())
        .collect();
    assert_eq!(expected, metric.get_value(&glean, "store1").unwrap());

    // Ensure the 21st string wasn't added.
    metric.add_sync(&glean, "test_string");
    assert_eq!(expected, metric.get_value(&glean, "store1").unwrap());

    // Ensure we recorded the error.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );

    // Try to set it to a list that's too long. Ensure it cuts off at 20 elements.
    let too_many: Vec<String> = "test_string "
        .repeat(21)
        .split_whitespace()
        .map(|s| s.to_string())
        .collect();
    metric.set_sync(&glean, too_many);
    assert_eq!(expected, metric.get_value(&glean, "store1").unwrap());

    assert_eq!(
        Ok(2),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue)
    );
}

#[test]
fn set_does_not_record_error_when_receiving_empty_list() {
    let (glean, _t) = new_glean(None);

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry.test".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set_sync(&glean, vec![]);

    // Ensure the empty list was added
    assert_eq!(Some(vec![]), metric.get_value(&glean, "store1"));

    // Ensure we didn't record an error.
    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).is_err());
}
