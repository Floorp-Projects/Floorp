// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::ClientInfoMetrics;
use crate::{Configuration, ConfigurationBuilder};
use std::sync::{Mutex, MutexGuard};

use once_cell::sync::Lazy;

pub(crate) const GLOBAL_APPLICATION_ID: &str = "org.mozilla.rlb.test";

// Because Glean uses a global-singleton, we need to run the tests one-by-one to
// avoid different tests stomping over each other.
// This is only an issue because we're resetting Glean, this cannot happen in normal
// use of the RLB.
//
// We use a global lock to force synchronization of all tests, even if run multi-threaded.
// This allows us to run without `--test-threads 1`.`
pub(crate) fn lock_test() -> MutexGuard<'static, ()> {
    static GLOBAL_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    // This is going to be called from all the tests: make sure
    // to enable logging.
    env_logger::try_init().ok();

    let lock = GLOBAL_LOCK.lock().unwrap();

    lock
}

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
pub(crate) fn new_glean(
    configuration: Option<Configuration>,
    clear_stores: bool,
) -> tempfile::TempDir {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = match configuration {
        Some(c) => c,
        None => ConfigurationBuilder::new(true, tmpname, GLOBAL_APPLICATION_ID)
            .with_server_endpoint("invalid-test-host")
            .build(),
    };

    crate::test_reset_glean(cfg, ClientInfoMetrics::unknown(), clear_stores);
    dir
}
