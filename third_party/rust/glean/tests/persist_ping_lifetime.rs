// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use glean::{ClientInfoMetrics, Configuration, ConfigurationBuilder};
use std::path::PathBuf;

/// Some user metrics.
mod metrics {
    use glean::private::*;
    use glean::Lifetime;
    use glean_core::CommonMetricData;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static boo: Lazy<BooleanMetric> = Lazy::new(|| {
        BooleanMetric::new(CommonMetricData {
            name: "boo".into(),
            category: "sample".into(),
            send_in_pings: vec!["validation".into()],
            lifetime: Lifetime::Ping,
            disabled: false,
            ..Default::default()
        })
    });
}

fn cfg_new(tmpname: PathBuf) -> Configuration {
    ConfigurationBuilder::new(true, tmpname, "firefox-desktop")
        .with_server_endpoint("invalid-test-host")
        .with_delay_ping_lifetime_io(true)
        .build()
}

/// Test scenario: Are ping-lifetime data persisted on shutdown when delayed?
///
/// delay_ping_lifetime_io: true has Glean put "ping"-lifetime data in-memory
/// instead of the db. Ensure that, on orderly shutdowns, we correctly persist
/// these in-memory data to the db.
#[test]
fn delayed_ping_data() {
    common::enable_test_logging();

    metrics::boo.set(true);

    // Create a custom configuration to delay ping-lifetime io
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    common::initialize(cfg_new(tmpname.clone()));

    assert!(
        metrics::boo.test_get_value(None).unwrap(),
        "Data should be present. Doesn't mean it's persisted, though."
    );

    glean::test_reset_glean(
        cfg_new(tmpname.clone()),
        ClientInfoMetrics::unknown(),
        false,
    );

    assert_eq!(
        None,
        metrics::boo.test_get_value(None),
        "Data should not have made it to disk on unclean shutdown."
    );
    metrics::boo.set(true); // Let's try again

    // This time, let's shut down cleanly
    glean::shutdown();

    // Now when we init, we should get the persisted data
    glean::test_reset_glean(cfg_new(tmpname), ClientInfoMetrics::unknown(), false);
    assert!(
        metrics::boo.test_get_value(None).unwrap(),
        "Data must be persisted between clean shutdown and init!"
    );

    glean::shutdown(); // Cleanly shut down at the end of the test.
}
