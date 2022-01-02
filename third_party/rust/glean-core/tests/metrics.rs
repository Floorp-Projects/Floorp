// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use glean_core::metrics::*;
use glean_core::CommonMetricData;

#[test]
fn stores_strings() {
    let (glean, _t) = new_glean(None);
    let metric = StringMetric::new(CommonMetricData::new("local", "string", "baseline"));

    assert_eq!(None, metric.test_get_value(&glean, "baseline"));

    metric.set(&glean, "telemetry");
    assert_eq!(
        "telemetry",
        metric.test_get_value(&glean, "baseline").unwrap()
    );
}

#[test]
fn stores_counters() {
    let (glean, _t) = new_glean(None);
    let metric = CounterMetric::new(CommonMetricData::new("local", "counter", "baseline"));

    assert_eq!(None, metric.test_get_value(&glean, "baseline"));

    metric.add(&glean, 1);
    assert_eq!(1, metric.test_get_value(&glean, "baseline").unwrap());

    metric.add(&glean, 2);
    assert_eq!(3, metric.test_get_value(&glean, "baseline").unwrap());
}
