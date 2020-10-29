// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, DatetimeMetric, Lifetime, TimeUnit};

use chrono::{FixedOffset, TimeZone};

#[test]
fn sets_datetime_value() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = DatetimeMetric::new(
        CommonMetricData {
            name: "datetime_metric".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Second,
    );

    assert_eq!(None, metric.test_get_value("store1"));

    let a_datetime = FixedOffset::east(5 * 3600)
        .ymd(2020, 05, 07)
        .and_hms(11, 58, 00);
    metric.set(Some(a_datetime));

    assert_eq!(
        "2020-05-07T11:58:00+05:00",
        metric.test_get_value("store1").unwrap()
    );
}

#[test]
fn datetime_ipc() {
    // DatetimeMetric doesn't support IPC.
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];
    let _raii = ipc::test_set_need_ipc(true);
    let child_metric = DatetimeMetric::new(
        CommonMetricData {
            name: "datetime_metric".into(),
            category: "ipc".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Second,
    );

    // Instrumentation calls do not panic.
    let a_datetime = FixedOffset::east(5 * 3600)
        .ymd(2020, 10, 13)
        .and_hms(16, 41, 00);
    child_metric.set(Some(a_datetime));

    // (They also shouldn't do anything,
    // but that's not something we can inspect in this test)

    // Need to catch the panic so that our RAIIs drop nicely.
    let result = std::panic::catch_unwind(move || {
        child_metric.test_get_value("store1");
    });
    assert!(result.is_err());
}
