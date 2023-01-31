// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::ConfigurationBuilder;

/// Some user metrics.
mod metrics {
    use glean::private::*;
    use glean::Lifetime;
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static rapid_counting: Lazy<CounterMetric> = Lazy::new(|| {
        CounterMetric::new(CommonMetricData {
            name: "rapid_counting".into(),
            category: "sample".into(),
            send_in_pings: vec!["metrics".into()],
            lifetime: Lifetime::Ping,
            disabled: false,
            ..Default::default()
        })
    });

    // This is a hack, but a good one:
    //
    // To avoid reaching into RLB internals we re-create the metric so we can look at it.
    #[allow(non_upper_case_globals)]
    pub static preinit_tasks_overflow: Lazy<CounterMetric> = Lazy::new(|| {
        CounterMetric::new(CommonMetricData {
            category: "glean.error".into(),
            name: "preinit_tasks_overflow".into(),
            send_in_pings: vec!["metrics".into()],
            lifetime: Lifetime::Ping,
            disabled: false,
            ..Default::default()
        })
    });
}

/// Test scenario: Lots of metric recordings before  init.
///
/// The app starts recording metrics before Glean is initialized.
/// Once initialized the recordings are processed and data is persisted.
/// The pre-init dispatcher queue records how many recordings over the limit it saw.
///
/// This is an integration test to avoid dealing with resetting the dispatcher.
#[test]
fn overflowing_the_task_queue_records_telemetry() {
    common::enable_test_logging();

    // Create a custom configuration to use a validating uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = ConfigurationBuilder::new(true, tmpname, "firefox-desktop")
        .with_server_endpoint("invalid-test-host")
        .build();

    // Insert a bunch of tasks to overflow the queue.
    for _ in 0..1010 {
        metrics::rapid_counting.add(1);
    }

    // Now initialize Glean
    common::initialize(cfg);

    assert_eq!(Some(1000), metrics::rapid_counting.test_get_value(None));

    // The metrics counts the total number of overflowing tasks,
    // (and the count of tasks in the queue when we overflowed: bug 1764573)
    // this might include Glean-internal tasks.
    let val = metrics::preinit_tasks_overflow
        .test_get_value(None)
        .unwrap();
    assert!(val >= 10);

    glean::shutdown();
}
