// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::{ConfigurationBuilder, ErrorType};
use std::time::Duration;

/// A timing_distribution
mod metrics {
    use glean::private::*;
    use glean::{Lifetime, TimeUnit};
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static boo: Lazy<TimingDistributionMetric> = Lazy::new(|| {
        TimingDistributionMetric::new(
            CommonMetricData {
                name: "boo".into(),
                category: "sample".into(),
                send_in_pings: vec!["validation".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Millisecond,
        )
    });
}

/// Test scenario: Ensure single-sample accumulation works.
#[test]
fn raw_duration_works() {
    common::enable_test_logging();

    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = ConfigurationBuilder::new(true, tmpname, "firefox-desktop")
        .with_server_endpoint("invalid-test-host")
        .build();
    common::initialize(cfg);

    metrics::boo.accumulate_raw_duration(Duration::from_secs(1));

    assert_eq!(
        1,
        metrics::boo.test_get_value(None).unwrap().count,
        "Single sample: single count"
    );

    // Let's check overflow.
    metrics::boo.accumulate_raw_duration(Duration::from_nanos(u64::MAX));

    assert_eq!(2, metrics::boo.test_get_value(None).unwrap().count);
    assert_eq!(
        1,
        metrics::boo.test_get_num_recorded_errors(ErrorType::InvalidOverflow)
    );

    glean::shutdown(); // Cleanly shut down at the end of the test.
}
