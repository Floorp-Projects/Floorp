// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, Lifetime, TimeUnit, TimingDistributionMetric};

#[test]
fn smoke_test_timing_distribution() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let metric = TimingDistributionMetric::new(
        CommonMetricData {
            name: "timing_distribution_metric".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        TimeUnit::Nanosecond,
    );

    let id = metric.start();
    // Stopping right away might not give us data, if the underlying clock source is not precise
    // enough.
    // So let's cancel and make sure nothing blows up.
    metric.cancel(id);

    // We can't inspect the values yet.
    assert_eq!(None, metric.test_get_value("store1"));
}

#[test]
fn timing_distribution_child() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let meta = CommonMetricData {
        name: "timing_distribution_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    };

    let parent_metric = TimingDistributionMetric::new(meta.clone(), TimeUnit::Nanosecond);
    let id = parent_metric.start();
    std::thread::sleep(std::time::Duration::from_millis(10));
    parent_metric.stop_and_accumulate(id);

    {
        // scope for need_ipc RAII
        let _raii = ipc::test_set_need_ipc(true);
        let child_metric = TimingDistributionMetric::new(meta.clone(), TimeUnit::Nanosecond);

        let id = child_metric.start();
        let id2 = child_metric.start();
        assert_ne!(id, id2);
        std::thread::sleep(std::time::Duration::from_millis(10));
        child_metric.stop_and_accumulate(id);

        child_metric.cancel(id2);
    }

    // TODO: implement replay. See bug 1646165.
    // For now let's ensure there's something in the buffer and replay doesn't error.
    let buf = ipc::take_buf().unwrap();
    assert!(buf.len() > 0);
    assert!(ipc::replay_from_buf(&buf).is_ok());
}
