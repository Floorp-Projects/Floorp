// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, Lifetime, StringListMetric};

#[test]
fn sets_string_list_value() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = StringListMetric::new(CommonMetricData {
        name: "string_list_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.set(vec!["test_string_value".to_string()]);
    metric.add("another test value");

    assert_eq!(
        vec!["test_string_value", "another test value"],
        metric.test_get_value("store1").unwrap()
    );
}

#[test]
fn string_list_ipc() {
    // StringListMetric supports IPC only for `add`, not `set`.
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let meta = CommonMetricData {
        name: "string metric".into(),
        category: "ipc".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    };

    {
        let _raii = ipc::test_set_need_ipc(true);
        let child_metric = StringListMetric::new(meta.clone());

        // Recording APIs do not panic, even when they don't work.
        child_metric.set(vec!["not gonna be set".to_string()]);

        child_metric.add("child_value");
        assert!(ipc::take_buf().unwrap().len() > 0);

        // Test APIs are allowed to panic, though.
        let result = std::panic::catch_unwind(move || {
            child_metric.test_get_value("store1");
        });
        assert!(result.is_err());
    }

    // TODO: implement replay. See bug 1646165.
    // Then perform the replay and assert we have the values from both "processes".
}
