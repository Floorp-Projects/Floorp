// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod common;
use crate::common::*;

use serde_json::json;

use glean_core::metrics::*;
use glean_core::storage::StorageManager;
use glean_core::{test_get_num_recorded_errors, ErrorType};
use glean_core::{CommonMetricData, Glean, Lifetime};

// Tests ported from glean-ac

#[test]
fn serializer_should_correctly_serialize_memory_distribution() {
    let (_t, tmpname) = tempdir();

    let memory_unit = MemoryUnit::Kilobyte;
    let kb = 1024;

    let cfg = glean_core::Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
    };

    {
        let glean = Glean::new(cfg.clone()).unwrap();

        let metric = MemoryDistributionMetric::new(
            CommonMetricData {
                name: "distribution".into(),
                category: "telemetry".into(),
                send_in_pings: vec!["store1".into()],
                disabled: false,
                lifetime: Lifetime::Ping,
                ..Default::default()
            },
            memory_unit,
        );

        metric.accumulate(&glean, 100_000);

        let val = metric
            .test_get_value(&glean, "store1")
            .expect("Value should be stored");

        assert_eq!(val.sum(), 100_000 * kb);
    }

    // Make a new Glean instance here, which should force reloading of the data from disk
    // so we can ensure it persisted, because it has User lifetime
    {
        let glean = Glean::new(cfg.clone()).unwrap();
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), "store1", true)
            .unwrap();

        assert_eq!(
            json!(100_000 * kb),
            snapshot["memory_distribution"]["telemetry.distribution"]["sum"]
        );
    }
}

#[test]
fn set_value_properly_sets_the_value_in_all_stores() {
    let (glean, _t) = new_glean();
    let store_names: Vec<String> = vec!["store1".into(), "store2".into()];

    let metric = MemoryDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: store_names.clone(),
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        MemoryUnit::Byte,
    );

    metric.accumulate(&glean, 100_000);

    for store_name in store_names {
        let snapshot = StorageManager
            .snapshot_as_json(glean.storage(), &store_name, true)
            .unwrap();

        assert_eq!(
            json!(100_000),
            snapshot["memory_distribution"]["telemetry.distribution"]["sum"]
        );
        assert_eq!(
            json!(1),
            snapshot["memory_distribution"]["telemetry.distribution"]["values"]["96785"]
        );
    }
}

// SKIPPED from glean-ac: memory distributions must not accumulate negative values
// This test doesn't apply to Rust, because we're using unsigned integers.

#[test]
fn the_accumulate_samples_api_correctly_stores_memory_values() {
    let (glean, _t) = new_glean();

    let metric = MemoryDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        MemoryUnit::Kilobyte,
    );

    // Accumulate the samples. We intentionally do not report
    // negative values to not trigger error reporting.
    metric.accumulate_samples_signed(&glean, [1, 2, 3].to_vec());

    let val = metric
        .test_get_value(&glean, "store1")
        .expect("Value should be stored");

    let kb = 1024;

    // Check that we got the right sum and number of samples.
    assert_eq!(val.sum(), 6 * kb);
    assert_eq!(val.count(), 3);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * kb)` for `i = 1..=3`.
    assert_eq!(1, val.values()[&1023]);
    assert_eq!(1, val.values()[&2047]);
    assert_eq!(1, val.values()[&3024]);

    // No errors should be reported.
    assert!(test_get_num_recorded_errors(
        &glean,
        metric.meta(),
        ErrorType::InvalidValue,
        Some("store1")
    )
    .is_err());
}

#[test]
fn the_accumulate_samples_api_correctly_handles_negative_values() {
    let (glean, _t) = new_glean();

    let metric = MemoryDistributionMetric::new(
        CommonMetricData {
            name: "distribution".into(),
            category: "telemetry".into(),
            send_in_pings: vec!["store1".into()],
            disabled: false,
            lifetime: Lifetime::Ping,
            ..Default::default()
        },
        MemoryUnit::Kilobyte,
    );

    // Accumulate the samples.
    metric.accumulate_samples_signed(&glean, [-1, 1, 2, 3].to_vec());

    let val = metric
        .test_get_value(&glean, "store1")
        .expect("Value should be stored");

    let kb = 1024;

    // Check that we got the right sum and number of samples.
    assert_eq!(val.sum(), 6 * kb);
    assert_eq!(val.count(), 3);

    // We should get a sample in 3 buckets.
    // These numbers are a bit magic, but they correspond to
    // `hist.sample_to_bucket_minimum(i * kb)` for `i = 1..=3`.
    assert_eq!(1, val.values()[&1023]);
    assert_eq!(1, val.values()[&2047]);
    assert_eq!(1, val.values()[&3024]);

    // 1 error should be reported.
    assert_eq!(
        Ok(1),
        test_get_num_recorded_errors(
            &glean,
            metric.meta(),
            ErrorType::InvalidValue,
            Some("store1")
        )
    );
}
