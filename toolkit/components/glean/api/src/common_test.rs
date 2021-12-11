// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::sync::{Mutex, MutexGuard};

use once_cell::sync::Lazy;

const GLOBAL_APPLICATION_ID: &str = "org.mozilla.firefox.test";

/// UGLY HACK.
/// We use a global lock to force synchronization of all tests, even if run multi-threaded.
/// This allows us to run without `--test-threads 1`.`
pub fn lock_test() -> (MutexGuard<'static, ()>, tempfile::TempDir) {
    static GLOBAL_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    let lock = GLOBAL_LOCK.lock().unwrap();

    let dir = setup_glean(None);
    (lock, dir)
}

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
fn setup_glean(tempdir: Option<tempfile::TempDir>) -> tempfile::TempDir {
    let dir = match tempdir {
        Some(tempdir) => tempdir,
        None => tempfile::tempdir().unwrap(),
    };
    let tmpname = dir.path().to_path_buf();

    let cfg = glean::Configuration {
        upload_enabled: true,
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: None,
        server_endpoint: None,
        uploader: None,
        use_core_mps: false,
    };

    let client_info = glean::ClientInfoMetrics {
        app_build: "test-build".into(),
        app_display_version: "1.2.3".into(),
    };

    glean::test_reset_glean(cfg, client_info, true);

    dir
}
