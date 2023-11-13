// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use std::collections::HashMap;

use glean_core::metrics::*;
use glean_core::CommonMetricData;
use glean_core::Lifetime;

#[test]
fn write_ping_to_disk() {
    let (mut glean, _temp) = new_glean(None);

    let ping = PingType::new("metrics", true, false, true, vec![]);
    glean.register_ping_type(&ping);

    // We need to store a metric as an empty ping is not stored.
    let counter = CounterMetric::new(CommonMetricData {
        name: "counter".into(),
        category: "local".into(),
        send_in_pings: vec!["metrics".into()],
        ..Default::default()
    });
    counter.add_sync(&glean, 1);

    assert!(ping.submit_sync(&glean, None));

    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn disabling_upload_clears_pending_pings() {
    let (mut glean, _t) = new_glean(None);

    let ping = PingType::new("metrics", true, false, true, vec![]);
    glean.register_ping_type(&ping);

    // We need to store a metric as an empty ping is not stored.
    let counter = CounterMetric::new(CommonMetricData {
        name: "counter".into(),
        category: "local".into(),
        send_in_pings: vec!["metrics".into()],
        ..Default::default()
    });

    counter.add_sync(&glean, 1);
    assert!(ping.submit_sync(&glean, None));
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());
    // At this point no deletion_request ping should exist
    // (that is: it's directory should not exist at all)
    assert!(get_deletion_pings(glean.get_data_path()).is_err());

    glean.set_upload_enabled(false);
    assert_eq!(0, get_queued_pings(glean.get_data_path()).unwrap().len());
    // Disabling upload generates a deletion ping
    let dpings = get_deletion_pings(glean.get_data_path()).unwrap();
    assert_eq!(1, dpings.len());
    let payload = &dpings[0].1;
    assert_eq!(
        "set_upload_enabled",
        payload["ping_info"].as_object().unwrap()["reason"]
            .as_str()
            .unwrap()
    );

    glean.set_upload_enabled(true);
    assert_eq!(0, get_queued_pings(glean.get_data_path()).unwrap().len());

    counter.add_sync(&glean, 1);
    assert!(ping.submit_sync(&glean, None));
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn deletion_request_only_when_toggled_from_on_to_off() {
    let (mut glean, _t) = new_glean(None);

    // Disabling upload generates a deletion ping
    glean.set_upload_enabled(false);
    let dpings = get_deletion_pings(glean.get_data_path()).unwrap();
    assert_eq!(1, dpings.len());
    let payload = &dpings[0].1;
    assert_eq!(
        "set_upload_enabled",
        payload["ping_info"].as_object().unwrap()["reason"]
            .as_str()
            .unwrap()
    );

    // Re-setting it to `false` should not generate an additional ping.
    // As we didn't clear the pending ping, that's the only one that sticks around.
    glean.set_upload_enabled(false);
    assert_eq!(1, get_deletion_pings(glean.get_data_path()).unwrap().len());

    // Toggling back to true won't generate a ping either.
    glean.set_upload_enabled(true);
    assert_eq!(1, get_deletion_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn empty_pings_with_flag_are_sent() {
    let (mut glean, _t) = new_glean(None);

    let ping1 = PingType::new("custom-ping1", true, true, true, vec![]);
    glean.register_ping_type(&ping1);
    let ping2 = PingType::new("custom-ping2", true, false, true, vec![]);
    glean.register_ping_type(&ping2);

    // No data is stored in either of the custom pings

    // Sending this should succeed.
    assert!(ping1.submit_sync(&glean, None));
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());

    // Sending this should fail.
    assert!(!ping2.submit_sync(&glean, None));
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn test_pings_submitted_metric() {
    let (mut glean, _temp) = new_glean(None);

    // Reconstructed here so we can test it without reaching into the library
    // internals.
    let pings_submitted = LabeledCounter::new(
        CommonMetricData {
            name: "pings_submitted".into(),
            category: "glean.validation".into(),
            send_in_pings: vec!["metrics".into(), "baseline".into()],
            lifetime: Lifetime::Ping,
            disabled: false,
            dynamic_label: None,
        },
        None,
    );

    let metrics_ping = PingType::new("metrics", true, false, true, vec![]);
    glean.register_ping_type(&metrics_ping);

    let baseline_ping = PingType::new("baseline", true, false, true, vec![]);
    glean.register_ping_type(&baseline_ping);

    // We need to store a metric as an empty ping is not stored.
    let counter = CounterMetric::new(CommonMetricData {
        name: "counter".into(),
        category: "local".into(),
        send_in_pings: vec!["metrics".into()],
        ..Default::default()
    });
    counter.add_sync(&glean, 1);

    assert!(metrics_ping.submit_sync(&glean, None));

    // Check recording in the metrics ping
    assert_eq!(
        Some(1),
        pings_submitted.get("metrics").get_value(&glean, "metrics")
    );
    assert_eq!(
        None,
        pings_submitted.get("baseline").get_value(&glean, "metrics")
    );

    // Check recording in the baseline ping
    assert_eq!(
        Some(1),
        pings_submitted.get("metrics").get_value(&glean, "baseline")
    );
    assert_eq!(
        None,
        pings_submitted
            .get("baseline")
            .get_value(&glean, "baseline")
    );

    // Trigger 2 baseline pings.
    // This should record a count of 2 baseline pings in the metrics ping, but
    // it resets each time on the baseline ping, so we should only ever get 1
    // baseline ping recorded in the baseline ping itsef.
    assert!(baseline_ping.submit_sync(&glean, None));
    assert!(baseline_ping.submit_sync(&glean, None));

    // Check recording in the metrics ping
    assert_eq!(
        Some(1),
        pings_submitted
            .get("metrics")
            .get_value(&glean, Some("metrics"))
    );
    assert_eq!(
        Some(2),
        pings_submitted
            .get("baseline")
            .get_value(&glean, Some("metrics"))
    );

    // Check recording in the baseline ping
    assert_eq!(
        None,
        pings_submitted
            .get("metrics")
            .get_value(&glean, Some("baseline"))
    );
    assert_eq!(
        Some(1),
        pings_submitted
            .get("baseline")
            .get_value(&glean, Some("baseline"))
    );
}

#[test]
fn events_ping_with_metric_but_no_events_is_not_sent() {
    let (mut glean, _t) = new_glean(None);

    let events_ping = PingType::new("events", true, true, true, vec![]);
    glean.register_ping_type(&events_ping);
    let counter = CounterMetric::new(CommonMetricData {
        name: "counter".into(),
        category: "local".into(),
        send_in_pings: vec!["events".into()],
        ..Default::default()
    });
    counter.add_sync(&glean, 1);

    // Sending this should fail.
    assert!(!events_ping.submit_sync(&glean, None));
    assert!(get_queued_pings(glean.get_data_path()).is_err());

    let event = EventMetric::new(
        CommonMetricData {
            name: "name".into(),
            category: "category".into(),
            send_in_pings: vec!["events".into()],
            ..Default::default()
        },
        vec![],
    );
    event.record_sync(&glean, 0, HashMap::new(), 0);

    // Sending this should now succeed.
    assert!(events_ping.submit_sync(&glean, None));
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());
}
