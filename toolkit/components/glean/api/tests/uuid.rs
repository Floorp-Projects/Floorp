// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use uuid::Uuid;

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, Lifetime, UuidMetric};

#[test]
fn sets_uuid_value() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = UuidMetric::new(CommonMetricData {
        name: "uuid_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    let expected = Uuid::new_v4();
    metric.set(expected.clone());

    assert_eq!(expected, metric.test_get_value("store1").unwrap());
}

#[test]
fn uuid_ipc() {
    // UuidMetric doesn't support IPC.
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];
    let _raii = ipc::test_set_need_ipc(true);
    let child_metric = UuidMetric::new(CommonMetricData {
        name: "uuid_metric".into(),
        category: "ipc".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    // Instrumentation calls do not panic.
    child_metric.set(Uuid::new_v4());

    // (They also shouldn't do anything,
    // but that's not something we can inspect in this test)

    // Need to catch the panic so that our RAIIs drop nicely.
    let result = std::panic::catch_unwind(move || {
        child_metric.test_get_value("store1");
    });
    assert!(result.is_err());
}
