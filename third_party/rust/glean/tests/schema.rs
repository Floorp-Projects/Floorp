// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::io::Read;

use flate2::read::GzDecoder;
use jsonschema_valid::{self, schemas::Draft};
use serde_json::Value;

use glean::private::{DenominatorMetric, NumeratorMetric, RateMetric};
use glean::{ClientInfoMetrics, CommonMetricData, Configuration};

const SCHEMA_JSON: &str = include_str!("../../../glean.1.schema.json");

fn load_schema() -> Value {
    serde_json::from_str(SCHEMA_JSON).unwrap()
}

const GLOBAL_APPLICATION_ID: &str = "org.mozilla.glean.test.app";

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
fn new_glean(configuration: Option<Configuration>) -> tempfile::TempDir {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = match configuration {
        Some(c) => c,
        None => Configuration {
            data_path: tmpname,
            application_id: GLOBAL_APPLICATION_ID.into(),
            upload_enabled: true,
            max_events: None,
            delay_ping_lifetime_io: false,
            channel: Some("testing".into()),
            server_endpoint: Some("invalid-test-host".into()),
            uploader: None,
            use_core_mps: false,
        },
    };

    let client_info = ClientInfoMetrics {
        app_build: env!("CARGO_PKG_VERSION").to_string(),
        app_display_version: env!("CARGO_PKG_VERSION").to_string(),
    };

    glean::initialize(cfg, client_info);

    dir
}

#[test]
fn validate_against_schema() {
    let schema = load_schema();

    let (s, r) = crossbeam_channel::bounded::<Vec<u8>>(1);

    // Define a fake uploader that reports back the submitted payload
    // using a crossbeam channel.
    #[derive(Debug)]
    pub struct ValidatingUploader {
        sender: crossbeam_channel::Sender<Vec<u8>>,
    }
    impl glean::net::PingUploader for ValidatingUploader {
        fn upload(
            &self,
            _url: String,
            body: Vec<u8>,
            _headers: Vec<(String, String)>,
        ) -> glean::net::UploadResult {
            self.sender.send(body).unwrap();
            glean::net::UploadResult::HttpStatus(200)
        }
    }

    // Create a custom configuration to use a validating uploader.
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().to_path_buf();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: Some("testing".into()),
        server_endpoint: Some("invalid-test-host".into()),
        uploader: Some(Box::new(ValidatingUploader { sender: s })),
        use_core_mps: false,
    };
    let _ = new_glean(Some(cfg));

    const PING_NAME: &str = "test-ping";

    // Test each of the metric types, just for basic smoke testing against the
    // schema

    // TODO: 1695762 Test all of the metric types against the schema from Rust

    let rate_metric: RateMetric = RateMetric::new(CommonMetricData {
        category: "test".into(),
        name: "rate".into(),
        send_in_pings: vec![PING_NAME.into()],
        ..Default::default()
    });
    rate_metric.add_to_numerator(1);
    rate_metric.add_to_denominator(1);

    let numerator_metric1: NumeratorMetric = NumeratorMetric::new(CommonMetricData {
        category: "test".into(),
        name: "num1".into(),
        send_in_pings: vec![PING_NAME.into()],
        ..Default::default()
    });
    let numerator_metric2: NumeratorMetric = NumeratorMetric::new(CommonMetricData {
        category: "test".into(),
        name: "num2".into(),
        send_in_pings: vec![PING_NAME.into()],
        ..Default::default()
    });
    let denominator_metric: DenominatorMetric = DenominatorMetric::new(
        CommonMetricData {
            category: "test".into(),
            name: "den".into(),
            send_in_pings: vec![PING_NAME.into()],
            ..Default::default()
        },
        vec![
            CommonMetricData {
                category: "test".into(),
                name: "num1".into(),
                send_in_pings: vec![PING_NAME.into()],
                ..Default::default()
            },
            CommonMetricData {
                category: "test".into(),
                name: "num2".into(),
                send_in_pings: vec![PING_NAME.into()],
                ..Default::default()
            },
        ],
    );

    numerator_metric1.add_to_numerator(1);
    numerator_metric2.add_to_numerator(2);
    denominator_metric.add(3);

    // Define a new ping and submit it.
    let custom_ping = glean::private::PingType::new(PING_NAME, true, true, vec![]);
    custom_ping.submit(None);

    // Wait for the ping to arrive.
    let raw_body = r.recv().unwrap();

    // Decode the gzipped body.
    let mut gzip_decoder = GzDecoder::new(&raw_body[..]);
    let mut s = String::with_capacity(raw_body.len());

    let data = gzip_decoder
        .read_to_string(&mut s)
        .ok()
        .map(|_| &s[..])
        .or_else(|| std::str::from_utf8(&raw_body).ok())
        .and_then(|payload| serde_json::from_str(payload).ok())
        .unwrap();

    // Now validate against the vendored schema
    let cfg = jsonschema_valid::Config::from_schema(&schema, Some(Draft::Draft6)).unwrap();
    let validation = cfg.validate(&data);
    match validation {
        Ok(()) => {}
        Err(e) => {
            let errors = e.map(|e| format!("{}", e)).collect::<Vec<_>>();
            panic!("Data: {:#?}\nErrors: {:#?}", data, errors);
        }
    }
}
