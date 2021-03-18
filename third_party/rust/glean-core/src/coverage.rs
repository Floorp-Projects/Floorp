// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Utilities for recording when testing APIs have been called on specific
//! metrics.
//!
//! Testing coverage is enabled by setting the GLEAN_TEST_COVERAGE environment
//! variable to the name of an output file. This output file must run through a
//! post-processor (in glean_parser's `coverage` command) to convert to a format
//! understood by third-party coverage reporting tools.
//!
//! While running a unit test suite, Glean records which database keys were
//! accessed by the testing APIs, with one entry per line. Database keys are
//! usually, but not always, the same as metric identifiers, but it is the
//! responsibility of the post-processor to resolve that difference.
//!
//! This functionality has no runtime overhead unless the testing API is used.

use std::env;
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

use once_cell::sync::Lazy;

static COVERAGE_FILE: Lazy<Option<Mutex<File>>> = Lazy::new(|| {
    if let Some(filename) = env::var_os("GLEAN_TEST_COVERAGE") {
        match OpenOptions::new().append(true).create(true).open(filename) {
            Ok(file) => {
                return Some(Mutex::new(file));
            }
            Err(err) => {
                log::error!("Couldn't open file for coverage results: {:?}", err);
            }
        }
    }
    None
});

pub(crate) fn record_coverage(metric_id: &str) {
    if let Some(file_mutex) = &*COVERAGE_FILE {
        let mut file = file_mutex.lock().unwrap();
        writeln!(&mut file, "{}", metric_id).ok();
        file.flush().ok();
    }
}
