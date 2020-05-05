// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use std::collections::HashMap;

use glean::metrics::{CommonMetricData, EventMetric, ExtraKeys, Lifetime, NoExtraKeys};

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

    // We roughly inspect the JSON-encoded values only.
    assert!(recorded.contains("event_metric"));
}

#[test]
fn smoke_test_event_with_extra() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    #[derive(Clone, Copy, Hash, Eq, PartialEq)]
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
        name: "event_metric".into(),
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

    // We roughly inspect the JSON-encoded values only.
    assert!(recorded.contains("event_metric"));
    assert!(recorded.contains("extra1"));
    assert!(recorded.contains("a-valid-value"));
}
