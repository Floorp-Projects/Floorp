// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use fog::ipc;
use fog::private::{CommonMetricData, Lifetime, MemoryDistributionMetric, MemoryUnit};

#[test]
fn smoke_test_memory_distribution() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let metric = MemoryDistributionMetric::new(
        CommonMetricData {
            name: "memory_distribution_metric".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        MemoryUnit::Kilobyte,
    );

    metric.accumulate(42);

    let metric_data = metric.test_get_value("store1").unwrap();
    assert_eq!(1, metric_data.values[&42494]);
    assert_eq!(0, metric_data.values[&44376]);
    assert_eq!(43008, metric_data.sum);
}

#[test]
fn memory_distribution_child() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let store_names: Vec<String> = vec!["store1".into()];
    let meta = CommonMetricData {
        name: "memory_distribution_metric".into(),
        category: "telemetry".into(),
        send_in_pings: store_names.clone(),
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    };
    let parent_metric = MemoryDistributionMetric::new(meta.clone(), MemoryUnit::Kilobyte);

    parent_metric.accumulate(42);

    {
        // scope for need_ipc RAII
        let _raii = ipc::test_set_need_ipc(true);
        let child_metric = MemoryDistributionMetric::new(meta.clone(), MemoryUnit::Kilobyte);

        child_metric.accumulate(13 * 9);
    }

    let metric_data = parent_metric.test_get_value("store1").unwrap();
    assert_eq!(1, metric_data.values[&42494]);
    assert_eq!(0, metric_data.values[&44376]);
    assert_eq!(43008, metric_data.sum);
    // TODO: implement replay. See bug 1646165.
    // For now, let's ensure there's something in the buffer and replay doesn't error.
    let buf = ipc::take_buf().unwrap();
    assert!(buf.len() > 0);
    assert!(ipc::replay_from_buf(&buf).is_ok());
}
