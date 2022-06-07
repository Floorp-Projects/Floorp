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
use std::path::PathBuf;

fn cfg_new(tmpname: PathBuf) -> Configuration {
    Configuration {
        data_path: tmpname,
        application_id: "firefox-desktop".into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: true,
        server_endpoint: Some("invalid-test-host".into()),
        uploader: None,
        use_core_mps: false,
    }
}

/// Test scenario: `persist_ping_lifetime_data` called after shutdown.
#[test]
fn delayed_ping_data() {
    common::enable_test_logging();

    // Create a custom configuration to delay ping-lifetime io
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    common::initialize(cfg_new(tmpname));
    glean::persist_ping_lifetime_data();

    glean::shutdown();
    glean::persist_ping_lifetime_data();
}
