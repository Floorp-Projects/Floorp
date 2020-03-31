// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use std::collections::HashMap;
use std::fs;

use glean_core::metrics::*;
use glean_core::{CommonMetricData, Lifetime};

#[test]
fn record_properly_records_without_optional_arguments() {
    let store_names = vec!["store1".into(), "store2".into()];

    let (glean, _t) = new_glean(None);

    let metric = EventMetric::new(
        CommonMetricData {
            name: "test_event_no_optional".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec![],
    );

    metric.record(&glean, 1000, None);

    for store_name in store_names {
        let events = metric.test_get_value(&glean, &store_name).unwrap();
        assert_eq!(1, events.len());
        assert_eq!("telemetry", events[0].category);
        assert_eq!("test_event_no_optional", events[0].name);
        assert!(events[0].extra.is_none());
    }
}

#[test]
fn record_properly_records_with_optional_arguments() {
    let (glean, _t) = new_glean(None);

    let store_names = vec!["store1".into(), "store2".into()];

    let metric = EventMetric::new(
        CommonMetricData {
            name: "test_event_no_optional".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec!["key1".into(), "key2".into()],
    );

    let extra: HashMap<i32, String> = [(0, "value1".into()), (1, "value2".into())]
        .iter()
        .cloned()
        .collect();

    metric.record(&glean, 1000, extra);

    for store_name in store_names {
        let events = metric.test_get_value(&glean, &store_name).unwrap();
        let event = events[0].clone();
        assert_eq!(1, events.len());
        assert_eq!("telemetry", event.category);
        assert_eq!("test_event_no_optional", event.name);
        let extra = event.extra.unwrap();
        assert_eq!(2, extra.len());
        assert_eq!("value1", extra["key1"]);
        assert_eq!("value2", extra["key2"]);
    }
}

// SKIPPED record() computes the correct time between events
// Timing is now handled in the language-specific part.

#[test]
fn snapshot_returns_none_if_nothing_is_recorded_in_the_store() {
    let (glean, _t) = new_glean(None);

    assert!(glean
        .event_storage()
        .snapshot_as_json("store1", false)
        .is_none())
}

#[test]
fn snapshot_correctly_clears_the_stores() {
    let (glean, _t) = new_glean(None);

    let store_names = vec!["store1".into(), "store2".into()];

    let metric = EventMetric::new(
        CommonMetricData {
            name: "test_event_clear".into(),
            category: "telemetry".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec![],
    );

    metric.record(&glean, 1000, None);

    let snapshot = glean.event_storage().snapshot_as_json("store1", true);
    assert!(snapshot.is_some());

    assert!(glean
        .event_storage()
        .snapshot_as_json("store1", false)
        .is_none());

    let files: Vec<fs::DirEntry> = fs::read_dir(&glean.event_storage().path)
        .unwrap()
        .filter_map(|x| x.ok())
        .collect();
    assert_eq!(1, files.len());
    assert_eq!("store2", files[0].file_name());

    let snapshot2 = glean.event_storage().snapshot_as_json("store2", false);
    for s in vec![snapshot, snapshot2] {
        assert!(s.is_some());
        let s = s.unwrap();
        assert_eq!(1, s.as_array().unwrap().len());
        assert_eq!("telemetry", s[0]["category"]);
        assert_eq!("test_event_clear", s[0]["name"]);
        println!("{:?}", s[0].get("extra"));
        assert!(s[0].get("extra").is_none());
    }
}

// SKIPPED: Events are serialized in the correct JSON format (no extra)
// SKIPPED: Events are serialized in the correct JSON format (with extra)
// This test won't work as-is since Rust doesn't maintain the insertion order in
// a JSON object, therefore you can't check the JSON output directly against a
// string.  This check is redundant with other tests, anyway, and checking against
// the schema is much more useful.

#[test]
fn test_sending_of_event_ping_when_it_fills_up() {
    let (mut glean, _t) = new_glean(None);

    let store_names: Vec<String> = vec!["events".into()];

    for store_name in &store_names {
        glean.register_ping_type(&PingType::new(store_name.clone(), true, false, vec![]));
    }

    let click = EventMetric::new(
        CommonMetricData {
            name: "click".into(),
            category: "ui".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec!["test_event_number".into()],
    );

    // We send 510 events. We expect to get the first 500 in the ping and 10
    // remaining afterward
    for i in 0..510 {
        let mut extra: HashMap<i32, String> = HashMap::new();
        extra.insert(0, i.to_string());
        click.record(&glean, i, extra);
    }

    assert_eq!(10, click.test_get_value(&glean, "events").unwrap().len());

    let (url, json) = &get_queued_pings(glean.get_data_path()).unwrap()[0];
    assert!(url.starts_with(format!("/submit/{}/events/", glean.get_application_id()).as_str()));
    assert_eq!(500, json["events"].as_array().unwrap().len());

    for i in 0..500 {
        let event = &json["events"].as_array().unwrap()[i];
        assert_eq!(i.to_string(), event["extra"]["test_event_number"]);
    }

    let snapshot = glean
        .event_storage()
        .snapshot_as_json("events", false)
        .unwrap();
    assert_eq!(10, snapshot.as_array().unwrap().len());
    for i in 0..10 {
        let event = &snapshot.as_array().unwrap()[i];
        assert_eq!((i + 500).to_string(), event["extra"]["test_event_number"]);
    }
}

#[test]
fn extra_keys_must_be_recorded_and_truncated_if_needed() {
    let (glean, _t) = new_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];

    let test_event = EventMetric::new(
        CommonMetricData {
            name: "testEvent".into(),
            category: "ui".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec!["extra1".into(), "truncatedExtra".into()],
    );

    let test_value = "LeanGleanByFrank";
    let mut extra: HashMap<i32, String> = HashMap::new();
    extra.insert(0, test_value.to_string());
    extra.insert(1, test_value.to_string().repeat(10));

    test_event.record(&glean, 0, extra);

    let snapshot = glean
        .event_storage()
        .snapshot_as_json("store1", false)
        .unwrap();
    assert_eq!(1, snapshot.as_array().unwrap().len());
    let event = &snapshot.as_array().unwrap()[0];
    assert_eq!("ui", event["category"]);
    assert_eq!("testEvent", event["name"]);
    assert_eq!(2, event["extra"].as_object().unwrap().len());
    assert_eq!(test_value, event["extra"]["extra1"]);
    assert_eq!(
        test_value.to_string().repeat(10)[0..100],
        event["extra"]["truncatedExtra"]
    );
}

#[test]
fn snapshot_sorts_the_timestamps() {
    let (glean, _t) = new_glean(None);

    let metric = EventMetric::new(
        CommonMetricData {
            name: "test_event_clear".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        vec![],
    );

    metric.record(&glean, 1000, None);
    metric.record(&glean, 100, None);
    metric.record(&glean, 10000, None);

    let snapshot = glean
        .event_storage()
        .snapshot_as_json("store1", true)
        .unwrap();

    assert_eq!(
        0,
        snapshot.as_array().unwrap()[0]["timestamp"]
            .as_i64()
            .unwrap()
    );
    assert_eq!(
        900,
        snapshot.as_array().unwrap()[1]["timestamp"]
            .as_i64()
            .unwrap()
    );
    assert_eq!(
        9900,
        snapshot.as_array().unwrap()[2]["timestamp"]
            .as_i64()
            .unwrap()
    );
}
