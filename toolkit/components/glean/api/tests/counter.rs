// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use glean::ipc;
use glean::private::{CommonMetricData, CounterMetric, Lifetime};

#[test]
fn sets_counter_value_parent() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric = CounterMetric::new(CommonMetricData {
        name: "counter_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    metric.add(1);

    assert_eq!(1, metric.test_get_value("store1").unwrap());
}

#[test]
fn sets_counter_value_child() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let meta = CommonMetricData {
        name: "counter metric".into(),
        category: "ipc".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    };
    assert!(
        false == ipc::need_ipc(),
        "Should start the test not needing ipc"
    );
    let parent_metric = CounterMetric::new(meta.clone());
    parent_metric.add(3);

    {
        // scope for need_ipc RAII
        let _raii = ipc::test_set_need_ipc(true);
        let child_metric = CounterMetric::new(meta.clone());

        child_metric.add(42);

        // Need to catch the panic so that our RAIIs drop nicely.
        let result = std::panic::catch_unwind(move || {
            child_metric.test_get_value("store1");
        });
        assert!(result.is_err());

        ipc::with_ipc_payload(move |payload| {
            let metric_id = ipc::MetricId::new(meta);
            assert!(
                42 == *payload.counters.get(&metric_id).unwrap(),
                "Stored the correct value in the ipc payload"
            );
        });
    }

    assert!(
        false == ipc::need_ipc(),
        "RAII dropped, should not need ipc any more"
    );
    assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());

    // TODO: implement replay. See bug 1646165.
    // assert!(45 == parent_metric.test_get_value("store1").unwrap(), "Values from the 'processes' should be summed");
}
