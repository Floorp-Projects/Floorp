// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use glean_core::metrics::*;
use glean_core::ping::PingMaker;
use glean_core::{CommonMetricData, Glean, Lifetime};

fn set_up_basic_ping() -> (Glean, PingMaker, PingType, tempfile::TempDir) {
    let (tempdir, _) = tempdir();
    let (mut glean, t) = new_glean(Some(tempdir));
    let ping_maker = PingMaker::new();
    let ping_type = PingType::new("store1", true, false, vec![]);
    glean.register_ping_type(&ping_type);

    // Record something, so the ping will have data
    let metric = BooleanMetric::new(CommonMetricData {
        name: "boolean_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::User,
        ..Default::default()
    });
    metric.set(&glean, true);

    (glean, ping_maker, ping_type, t)
}

#[test]
fn ping_info_must_contain_a_nonempty_start_and_end_time() {
    let (glean, ping_maker, ping_type, _t) = set_up_basic_ping();

    let ping = ping_maker
        .collect(&glean, &ping_type, None, "", "")
        .unwrap();
    let ping_info = ping.content["ping_info"].as_object().unwrap();

    let start_time_str = ping_info["start_time"].as_str().unwrap();
    let start_time_date = iso8601_to_chrono(&iso8601::datetime(start_time_str).unwrap());

    let end_time_str = ping_info["end_time"].as_str().unwrap();
    let end_time_date = iso8601_to_chrono(&iso8601::datetime(end_time_str).unwrap());

    assert!(start_time_date <= end_time_date);
}

#[test]
fn get_ping_info_must_report_all_the_required_fields() {
    let (glean, ping_maker, ping_type, _t) = set_up_basic_ping();

    let ping = ping_maker
        .collect(&glean, &ping_type, None, "", "")
        .unwrap();
    let ping_info = ping.content["ping_info"].as_object().unwrap();

    assert!(ping_info.get("start_time").is_some());
    assert!(ping_info.get("end_time").is_some());
    assert!(ping_info.get("seq").is_some());
}

#[test]
fn get_client_info_must_report_all_the_available_data() {
    let (glean, ping_maker, ping_type, _t) = set_up_basic_ping();

    let ping = ping_maker
        .collect(&glean, &ping_type, None, "", "")
        .unwrap();
    let client_info = ping.content["client_info"].as_object().unwrap();

    client_info["telemetry_sdk_build"].as_str().unwrap();
}

#[test]
fn collect_must_report_none_when_no_data_is_stored() {
    // NOTE: This is a behavior change from glean-ac which returned an empty
    // string in this case. As this is an implementation detail and not part of
    // the public API, it's safe to change this.

    let (mut glean, ping_maker, ping_type, _t) = set_up_basic_ping();

    let unknown_ping_type = PingType::new("unknown", true, false, vec![]);
    glean.register_ping_type(&ping_type);

    assert!(ping_maker
        .collect(&glean, &unknown_ping_type, None, "", "")
        .is_none());
}

#[test]
fn seq_number_must_be_sequential() {
    let (glean, ping_maker, _ping_type, _t) = set_up_basic_ping();

    let metric = BooleanMetric::new(CommonMetricData {
        name: "boolean_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store2".into()],
        disabled: false,
        lifetime: Lifetime::User,
        ..Default::default()
    });
    metric.set(&glean, true);

    for i in 0..=1 {
        for ping_name in ["store1", "store2"].iter() {
            let ping_type = PingType::new(*ping_name, true, false, vec![]);
            let ping = ping_maker
                .collect(&glean, &ping_type, None, "", "")
                .unwrap();
            let seq_num = ping.content["ping_info"]["seq"].as_i64().unwrap();
            // Ensure sequence numbers in different stores are independent of
            // each other
            assert_eq!(i, seq_num);
        }
    }

    // Test that ping sequence numbers increase independently.
    {
        let ping_type = PingType::new("store1", true, false, vec![]);

        // 3rd ping of store1
        let ping = ping_maker
            .collect(&glean, &ping_type, None, "", "")
            .unwrap();
        let seq_num = ping.content["ping_info"]["seq"].as_i64().unwrap();
        assert_eq!(2, seq_num);

        // 4th ping of store1
        let ping = ping_maker
            .collect(&glean, &ping_type, None, "", "")
            .unwrap();
        let seq_num = ping.content["ping_info"]["seq"].as_i64().unwrap();
        assert_eq!(3, seq_num);
    }

    {
        let ping_type = PingType::new("store2", true, false, vec![]);

        // 3rd ping of store2
        let ping = ping_maker
            .collect(&glean, &ping_type, None, "", "")
            .unwrap();
        let seq_num = ping.content["ping_info"]["seq"].as_i64().unwrap();
        assert_eq!(2, seq_num);
    }

    {
        let ping_type = PingType::new("store1", true, false, vec![]);

        // 5th ping of store1
        let ping = ping_maker
            .collect(&glean, &ping_type, None, "", "")
            .unwrap();
        let seq_num = ping.content["ping_info"]["seq"].as_i64().unwrap();
        assert_eq!(4, seq_num);
    }
}

#[test]
fn clear_pending_pings() {
    let (mut glean, _) = new_glean(None);
    let ping_maker = PingMaker::new();
    let ping_type = PingType::new("store1", true, false, vec![]);
    glean.register_ping_type(&ping_type);

    // Record something, so the ping will have data
    let metric = BooleanMetric::new(CommonMetricData {
        name: "boolean_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::User,
        ..Default::default()
    });
    metric.set(&glean, true);

    assert!(glean.submit_ping(&ping_type, None).is_ok());
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());

    assert!(ping_maker
        .clear_pending_pings(glean.get_data_path())
        .is_ok());
    assert_eq!(0, get_queued_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn no_pings_submitted_if_upload_disabled() {
    // Regression test, bug 1603571

    let (mut glean, _) = new_glean(None);
    let ping_type = PingType::new("store1", true, true, vec![]);
    glean.register_ping_type(&ping_type);

    assert!(glean.submit_ping(&ping_type, None).is_ok());
    assert_eq!(1, get_queued_pings(glean.get_data_path()).unwrap().len());

    // Disable upload, then try to sumbit
    glean.set_upload_enabled(false);

    assert!(glean.submit_ping(&ping_type, None).is_ok());
    assert_eq!(0, get_queued_pings(glean.get_data_path()).unwrap().len());

    // Test again through the direct call
    assert!(ping_type.submit(&glean, None).is_ok());
    assert_eq!(0, get_queued_pings(glean.get_data_path()).unwrap().len());
}

#[test]
fn metadata_is_correctly_added_when_necessary() {
    let (mut glean, _) = new_glean(None);
    glean.set_debug_view_tag("valid-tag");
    let ping_type = PingType::new("store1", true, true, vec![]);
    glean.register_ping_type(&ping_type);

    assert!(glean.submit_ping(&ping_type, None).is_ok());

    let (_, _, metadata) = &get_queued_pings(glean.get_data_path()).unwrap()[0];
    let headers = metadata.as_ref().unwrap().get("headers").unwrap();
    assert_eq!(headers.get("X-Debug-ID").unwrap(), "valid-tag");
}
