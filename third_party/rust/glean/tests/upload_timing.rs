// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! This integration test should model how the RLB is used when embedded in another Rust application
//! (e.g. FOG/Firefox Desktop).
//!
//! We write a single test scenario per file to avoid any state keeping across runs
//! (different files run as different processes).

mod common;

use std::io::Read;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::thread;
use std::time;

use crossbeam_channel::{bounded, Sender};
use flate2::read::GzDecoder;
use serde_json::Value as JsonValue;

use glean::net;
use glean::ConfigurationBuilder;

pub mod metrics {
    #![allow(non_upper_case_globals)]

    use glean::{
        private::BooleanMetric, private::TimingDistributionMetric, CommonMetricData, Lifetime,
        TimeUnit,
    };

    pub static sample_boolean: once_cell::sync::Lazy<BooleanMetric> =
        once_cell::sync::Lazy::new(|| {
            BooleanMetric::new(CommonMetricData {
                name: "sample_boolean".into(),
                category: "test.metrics".into(),
                send_in_pings: vec!["validation".into()],
                disabled: false,
                lifetime: Lifetime::Ping,
                ..Default::default()
            })
        });

    // The following are duplicated from `glean-core/src/internal_metrics.rs`
    // so we can use the test APIs to query them.

    pub static send_success: once_cell::sync::Lazy<TimingDistributionMetric> =
        once_cell::sync::Lazy::new(|| {
            TimingDistributionMetric::new(
                CommonMetricData {
                    name: "send_success".into(),
                    category: "glean.upload".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                TimeUnit::Millisecond,
            )
        });

    pub static send_failure: once_cell::sync::Lazy<TimingDistributionMetric> =
        once_cell::sync::Lazy::new(|| {
            TimingDistributionMetric::new(
                CommonMetricData {
                    name: "send_failure".into(),
                    category: "glean.upload".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                TimeUnit::Millisecond,
            )
        });

    pub static shutdown_wait: once_cell::sync::Lazy<TimingDistributionMetric> =
        once_cell::sync::Lazy::new(|| {
            TimingDistributionMetric::new(
                CommonMetricData {
                    name: "shutdown_wait".into(),
                    category: "glean.validation".into(),
                    send_in_pings: vec!["metrics".into()],
                    lifetime: Lifetime::Ping,
                    disabled: false,
                    dynamic_label: None,
                },
                TimeUnit::Millisecond,
            )
        });
}

mod pings {
    use glean::private::PingType;
    use once_cell::sync::Lazy;

    #[allow(non_upper_case_globals)]
    pub static validation: Lazy<PingType> =
        Lazy::new(|| glean::private::PingType::new("validation", true, true, true, vec![]));
}

// Define a fake uploader that sleeps.
#[derive(Debug)]
struct FakeUploader {
    calls: AtomicUsize,
    sender: Sender<JsonValue>,
}

impl net::PingUploader for FakeUploader {
    fn upload(
        &self,
        _url: String,
        body: Vec<u8>,
        _headers: Vec<(String, String)>,
    ) -> net::UploadResult {
        let calls = self.calls.fetch_add(1, Ordering::SeqCst);
        let decode = |body: Vec<u8>| {
            let mut gzip_decoder = GzDecoder::new(&body[..]);
            let mut s = String::with_capacity(body.len());

            gzip_decoder
                .read_to_string(&mut s)
                .ok()
                .map(|_| &s[..])
                .or_else(|| std::str::from_utf8(&body).ok())
                .and_then(|payload| serde_json::from_str(payload).ok())
                .unwrap()
        };

        match calls {
            // First goes through as is.
            0 => net::UploadResult::http_status(200),
            // Second briefly sleeps
            1 => {
                thread::sleep(time::Duration::from_millis(100));
                net::UploadResult::http_status(200)
            }
            // Third one fails
            2 => net::UploadResult::http_status(404),
            // Fourth one fast again
            3 => {
                self.sender.send(decode(body)).unwrap();
                net::UploadResult::http_status(200)
            }
            // Last one is the metrics ping, a-ok.
            _ => {
                self.sender.send(decode(body)).unwrap();
                net::UploadResult::http_status(200)
            }
        }
    }
}

/// Test scenario: Different timings for upload on success and failure.
///
/// The app is initialized, in turn Glean gets initialized without problems.
/// A custom ping is submitted multiple times to trigger upload.
/// A metrics ping is submitted to get the upload timing data.
///
/// And later the whole process is shutdown.
#[test]
fn upload_timings() {
    common::enable_test_logging();

    // Create a custom configuration to use a validating uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();
    let (tx, rx) = bounded(1);

    let cfg = ConfigurationBuilder::new(true, tmpname.clone(), "glean-upload-timing")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .with_uploader(FakeUploader {
            calls: AtomicUsize::new(0),
            sender: tx,
        })
        .build();
    common::initialize(cfg);

    // Wait for init to finish,
    // otherwise we might be to quick with calling `shutdown`.
    let _ = metrics::sample_boolean.test_get_value(None);

    // fast
    pings::validation.submit(None);
    // slow
    pings::validation.submit(None);
    // failed
    pings::validation.submit(None);
    // fast
    pings::validation.submit(None);

    // wait for the last ping
    let _body = rx.recv().unwrap();

    assert_eq!(
        3,
        metrics::send_success.test_get_value(None).unwrap().count,
        "Successful pings: two fast, one slow"
    );
    assert_eq!(
        1,
        metrics::send_failure.test_get_value(None).unwrap().count,
        "One failed ping"
    );

    // This is awkward, but it's what gets us very close to just starting a new process with a
    // fresh Glean.
    // This also calls `glean::shutdown();` internally, waiting on the uploader.
    let data_path = Some(tmpname.display().to_string());
    glean_core::glean_test_destroy_glean(false, data_path);

    let cfg = ConfigurationBuilder::new(true, tmpname, "glean-upload-timing")
        .with_server_endpoint("invalid-test-host")
        .with_use_core_mps(false)
        .build();
    common::initialize(cfg);

    assert_eq!(
        1,
        metrics::shutdown_wait.test_get_value(None).unwrap().count,
        "Measured time waiting for shutdown exactly once"
    );
}
