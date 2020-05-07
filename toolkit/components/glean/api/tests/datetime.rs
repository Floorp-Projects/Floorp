// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use glean::metrics::{CommonMetricData, DatetimeMetric, Lifetime, TimeUnit};

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
