// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::Configuration;

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
    pub static validation: Lazy<PingType> =
        Lazy::new(|| glean::private::PingType::new("validation", true, true, vec![]));
}

/// Test scenario: A clean run
///
/// The app is initialized, in turn Glean gets initialized without problems.
/// Some data is recorded (before and after initialization).
/// And later the whole process is shutdown.
#[test]
fn simple_lifecycle() {
    common::enable_test_logging();

    metrics::initialization.start();

    // Create a custom configuration to use a validating uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: "firefox-desktop".into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
    };
    common::initialize(cfg);

    metrics::initialization.stop();

    // This would never be called outside of tests,
    // but it's the only way we can really test it's working right now.
    assert!(metrics::initialization.test_get_value(None).is_some());

    pings::validation.submit(None);
    assert!(metrics::initialization.test_get_value(None).is_none());

    glean::shutdown();
}
