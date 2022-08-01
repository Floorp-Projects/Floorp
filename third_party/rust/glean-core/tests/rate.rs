// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use glean_core::metrics::*;
use glean_core::CommonMetricData;
use glean_core::{test_get_num_recorded_errors, ErrorType};

#[test]
fn rate_smoke() {
    let (glean, _t) = new_glean(None);

    let metric: RateMetric = RateMetric::new(CommonMetricData {
        name: "rate".into(),
        category: "test".into(),
        send_in_pings: vec!["test1".into()],
        ..Default::default()
    });

    // Adding 0 doesn't error.
    metric.add_to_numerator_sync(&glean, 0);
    metric.add_to_denominator_sync(&glean, 0);

    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).is_err());

    // Adding a negative value errors.
    metric.add_to_numerator_sync(&glean, -1);
    metric.add_to_denominator_sync(&glean, -1);

    assert_eq!(
        Ok(2),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue),
    );

    // Getting the value returns 0s if that's all we have.
    assert_eq!(metric.get_value(&glean, None), Some((0, 0).into()));

    // And normal values of course work.
    metric.add_to_numerator_sync(&glean, 22);
    metric.add_to_denominator_sync(&glean, 7);

    assert_eq!(metric.get_value(&glean, None), Some((22, 7).into()));
}

#[test]
fn numerator_smoke() {
    let (glean, _t) = new_glean(None);

    let metric: NumeratorMetric = NumeratorMetric::new(CommonMetricData {
        name: "rate".into(),
        category: "test".into(),
        send_in_pings: vec!["test1".into()],
        ..Default::default()
    });

    // Adding 0 doesn't error.
    metric.add_to_numerator_sync(&glean, 0);

    assert!(test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue).is_err());

    // Adding a negative value errors.
    metric.add_to_numerator_sync(&glean, -1);

    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(&glean, metric.meta(), ErrorType::InvalidValue),
    );

    // Getting the value returns 0s if that's all we have.
    let data = metric.get_value(&glean, None).unwrap();
    assert_eq!(0, data.numerator);
    assert_eq!(0, data.denominator);

    // And normal values of course work.
    metric.add_to_numerator_sync(&glean, 22);

    let data = metric.get_value(&glean, None).unwrap();
    assert_eq!(22, data.numerator);
    assert_eq!(0, data.denominator);
}

#[test]
fn denominator_smoke() {
    let (glean, _t) = new_glean(None);

    let meta1 = CommonMetricData {
        name: "rate1".into(),
        category: "test".into(),
        send_in_pings: vec!["test1".into()],
        ..Default::default()
    };

    let meta2 = CommonMetricData {
        name: "rate2".into(),
        category: "test".into(),
        send_in_pings: vec!["test1".into()],
        ..Default::default()
    };

    // This acts like a normal counter.
    let denom: DenominatorMetric = DenominatorMetric::new(
        CommonMetricData {
            name: "counter".into(),
            category: "test".into(),
            send_in_pings: vec!["test1".into()],
            ..Default::default()
        },
        vec![meta1.clone(), meta2.clone()],
    );

    let num1 = NumeratorMetric::new(meta1);
    let num2 = NumeratorMetric::new(meta2);

    num1.add_to_numerator_sync(&glean, 3);
    num2.add_to_numerator_sync(&glean, 5);

    denom.add_sync(&glean, 7);

    // no errors.
    assert!(test_get_num_recorded_errors(&glean, num1.meta(), ErrorType::InvalidValue).is_err());
    assert!(test_get_num_recorded_errors(&glean, num2.meta(), ErrorType::InvalidValue).is_err());

    // Getting the value returns 0s if that's all we have.
    let data = num1.get_value(&glean, None).unwrap();
    assert_eq!(3, data.numerator);
    assert_eq!(7, data.denominator);

    let data = num2.get_value(&glean, None).unwrap();
    assert_eq!(5, data.numerator);
    assert_eq!(7, data.denominator);
}
