// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, Lifetime, TimeUnit, TimespanMetric};

#[test]
fn smoke_test_timespan() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    metric.start();
    // Stopping right away might not give us data, if the underlying clock source is not precise
    // enough.
    // So let's cancel and make sure nothing blows up.
    metric.cancel();

    assert_eq!(None, metric.test_get_value("store1"));
}

#[test]
fn timespan_ipc() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let _raii = ipc::test_set_need_ipc(true);
    let child_metric = TimespanMetric::new(
        CommonMetricData {
            name: "timespan_metric".into(),
            category: "ipc".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    // Instrumentation calls do not panic.
    child_metric.start();
    // Stopping right away might not give us data,
    // if the underlying clock source is not precise enough.
    // So let's cancel and make sure nothing blows up.
    child_metric.cancel();

    // (They also shouldn't do anything,
    // but that's not something we can inspect in this test)

    // Need to catch the panic so that our RAIIs drop nicely.
    let result = std::panic::catch_unwind(move || {
        child_metric.test_get_value("store1");
    });
    assert!(result.is_err());
}
