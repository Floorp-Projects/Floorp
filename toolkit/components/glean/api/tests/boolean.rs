// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{BooleanMetric, CommonMetricData, Lifetime};

#[test]
fn sets_boolean_value() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = BooleanMetric::new(CommonMetricData {
        name: "boolean_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set(true);

    assert!(metric.test_get_value("store1").unwrap());
}

#[test]
fn boolean_ipc() {
    // BooleanMetric doesn't support IPC.
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];
    let _raii = ipc::test_set_need_ipc(true);
    let child_metric = BooleanMetric::new(CommonMetricData {
        name: "boolean_metric".into(),
        category: "ipc".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    // Instrumentation calls do not panic.
    child_metric.set(true);

    // (They also shouldn't do anything,
    // but that's not something we can inspect in this test)

    // Need to catch the panic so that our RAIIs drop nicely.
    let result = std::panic::catch_unwind(move || {
        child_metric.test_get_value("store1");
    });
    assert!(result.is_err());
}
