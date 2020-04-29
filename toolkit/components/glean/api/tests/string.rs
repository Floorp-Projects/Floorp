// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use glean::metrics::{CommonMetricData, Lifetime, StringMetric};

#[test]
fn sets_string_value() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = StringMetric::new(CommonMetricData {
        name: "string_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set("test_string_value");

    assert_eq!(
        "test_string_value",
        metric.test_get_value("store1").unwrap()
    );
}
