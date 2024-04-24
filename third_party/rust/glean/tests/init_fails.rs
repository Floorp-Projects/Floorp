// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use std::{thread, time::Duration};

use glean::ConfigurationBuilder;

/// Some user metrics.
mod metrics {
    use glean::private::*;
    use glean::{Lifetime, TimeUnit};
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static initialization: Lazy<TimespanMetric> = Lazy::new(|| {
        TimespanMetric::new(
            CommonMetricData {
                name: "initialization".into(),
                category: "sample".into(),
                send_in_pings: vec!["validation".into()],
                lifetime: Lifetime::Ping,
                disabled: false,
                ..Default::default()
            },
            TimeUnit::Nanosecond,
        )
    });
}

mod pings {
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static validation: Lazy<PingType> = Lazy::new(|| {
        glean::private::PingType::new("validation", true, true, true, true, true, vec![], vec![])
    });
}

/// Test scenario: Glean initialization fails.
///
/// App tries to initialize Glean, but that somehow fails.
#[test]
fn init_fails() {
    common::enable_test_logging();

    metrics::initialization.start();

    // Create a custom configuration to use a validating uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = ConfigurationBuilder::new(true, tmpname, "")
        .with_server_endpoint("invalid-test-host")
        .build();
    common::initialize(cfg);

    metrics::initialization.stop();

    pings::validation.submit(None);

    // We don't test for data here, as that would block on the dispatcher.

    // Give it a short amount of time to actually finish initialization.
    thread::sleep(Duration::from_millis(500));

    glean::shutdown();
}
