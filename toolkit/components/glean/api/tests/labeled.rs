// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use common::*;

use once_cell::sync::Lazy;

use glean::metrics::{BooleanMetric, CommonMetricData, ErrorType, LabeledMetric, Lifetime};

// Smoke test for what should be the generated code.
static GLOBAL_METRIC: Lazy<LabeledMetric<BooleanMetric>> = Lazy::new(|| {
    LabeledMetric::new(
        CommonMetricData {
            name: "global".into(),
            category: "metric".into(),
            send_in_pings: vec!["ping".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        None,
    )
});

#[test]
fn smoke_test_global_metric() {
    let _lock = lock_test();
    let _t = setup_glean(None);

    GLOBAL_METRIC.get("a_value").set(true);
    assert_eq!(
        true,
        GLOBAL_METRIC.get("a_value").test_get_value("ping").unwrap()
    );
}

#[test]
fn sets_labeled_bool_metrics() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric: LabeledMetric<BooleanMetric> = LabeledMetric::new(
        CommonMetricData {
            name: "bool".into(),
            category: "labeled".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        None,
    );

    metric.get("upload").set(true);

    assert!(metric.get("upload").test_get_value("store1").unwrap());
    assert_eq!(None, metric.get("download").test_get_value("store1"));
}

#[test]
fn records_errors() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric: LabeledMetric<BooleanMetric> = LabeledMetric::new(
        CommonMetricData {
            name: "bool".into(),
            category: "labeled".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        None,
    );

    metric
        .get("this_string_has_more_than_thirty_characters")
        .set(true);

    assert_eq!(
        Ok(1),
        metric.test_get_num_recorded_errors(ErrorType::InvalidLabel, None)
    );
}

#[test]
fn predefined_labels() {
    let _lock = lock_test();
    let _t = setup_glean(None);
    let store_names: Vec<String> = vec!["store1".into()];

    let metric: LabeledMetric<BooleanMetric> = LabeledMetric::new(
        CommonMetricData {
            name: "bool".into(),
            category: "labeled".into(),
            send_in_pings: store_names,
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        Some(vec!["label1".into(), "label2".into()]),
    );

    metric.get("label1").set(true);
    metric.get("label2").set(false);
    metric.get("not_a_label").set(true);

    assert_eq!(true, metric.get("label1").test_get_value("store1").unwrap());
    assert_eq!(
        false,
        metric.get("label2").test_get_value("store1").unwrap()
    );
    // The label not in the predefined set is recorded to the `other` bucket.
    assert_eq!(
        true,
        metric.get("__other__").test_get_value("store1").unwrap()
    );

    assert!(metric
        .test_get_num_recorded_errors(ErrorType::InvalidLabel, None)
        .is_err());
}
