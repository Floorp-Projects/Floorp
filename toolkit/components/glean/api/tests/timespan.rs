// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use glean::metrics::{CommonMetricData, Lifetime, TimeUnit, TimespanMetric};

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
