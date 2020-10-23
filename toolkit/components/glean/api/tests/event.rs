// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use std::collections::HashMap;

use fog::ipc;
use fog::private::{
    CommonMetricData, EventMetric, ExtraKeys, Lifetime, NoExtraKeys, RecordedEvent,
};

#[test]
fn smoke_test_event() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    let metric = EventMetric::<NoExtraKeys>::new(CommonMetricData {
        name: "event_metric".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    // No extra keys
    metric.record(None);

    let recorded = metric.test_get_value("store1").unwrap();

    assert!(recorded.iter().any(|e| e.name == "event_metric"));
}

#[test]
fn smoke_test_event_with_extra() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    #[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
    enum TestKeys {
        Extra1,
    }

    impl ExtraKeys for TestKeys {
        const ALLOWED_KEYS: &'static [&'static str] = &["extra1"];

        fn index(self) -> i32 {
            self as i32
        }
    }

    let metric = EventMetric::<TestKeys>::new(CommonMetricData {
        name: "event_metric_with_extra".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    });

    // No extra keys
    metric.record(None);

    // A valid extra key
    let mut map = HashMap::new();
    map.insert(TestKeys::Extra1, "a-valid-value".into());
    metric.record(map);

    let recorded = metric.test_get_value("store1").unwrap();

    let events: Vec<&RecordedEvent> = recorded
        .iter()
        .filter(|&e| e.category == "telemetry" && e.name == "event_metric_with_extra")
        .collect();
    assert_eq!(events.len(), 2);
    assert_eq!(events[0].extra, None);
    assert!(events[1].extra.as_ref().unwrap().get("extra1").unwrap() == "a-valid-value");
}

#[test]
fn event_ipc() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    #[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
    enum TestKeys {
        Extra1,
    }

    impl ExtraKeys for TestKeys {
        const ALLOWED_KEYS: &'static [&'static str] = &["extra1"];

        fn index(self) -> i32 {
            self as i32
        }
    }

    let meta = CommonMetricData {
        name: "event_metric_ipc".into(),
        category: "telemetry".into(),
        send_in_pings: vec!["store1".into()],
        disabled: false,
        lifetime: Lifetime::Ping,
        ..Default::default()
    };
    let parent_metric = EventMetric::<TestKeys>::new(meta.clone());

    // No extra keys
    parent_metric.record(None);

    {
        // scope for need_ipc RAII.
        let _raii = ipc::test_set_need_ipc(true);
        let child_metric = EventMetric::<TestKeys>::new(meta.clone());

        child_metric.record(None);

        let mut map = HashMap::new();
        map.insert(TestKeys::Extra1, "a-child-value".into());
        child_metric.record(map);
    }

    // Record in the parent after the child.
    let mut map = HashMap::new();
    map.insert(TestKeys::Extra1, "a-valid-value".into());
    parent_metric.record(map);

    let recorded = parent_metric.test_get_value("store1").unwrap();

    let events: Vec<&RecordedEvent> = recorded
        .iter()
        .filter(|&e| e.category == "telemetry" && e.name == "event_metric_ipc")
        .collect();
    assert_eq!(events.len(), 2);
    assert_eq!(events[0].extra, None);
    assert!(events[1].extra.as_ref().unwrap().get("extra1").unwrap() == "a-valid-value");
    // TODO: implement replay. See bug 1646165. Then change and add asserts for a-child-value.
    // Ensure the replay values apply without error, at least.
    let buf = ipc::take_buf().unwrap();
    assert!(buf.len() > 0);
    assert!(ipc::replay_from_buf(&ipc::take_buf().unwrap()).is_ok());
}
