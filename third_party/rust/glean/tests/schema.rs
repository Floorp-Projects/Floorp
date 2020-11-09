// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::PathBuf;

use jsonschema_valid::{self, schemas::Draft};
use serde_json::Value;

use glean::{private::PingType, ClientInfoMetrics, Configuration};

const SCHEMA_JSON: &str = include_str!("../../../glean.1.schema.json");

fn load_schema() -> Value {
    serde_json::from_str(SCHEMA_JSON).unwrap()
}

const GLOBAL_APPLICATION_ID: &str = "org.mozilla.glean.test.app";

// Create a new instance of Glean with a temporary directory.
// We need to keep the `TempDir` alive, so that it's not deleted before we stop using it.
fn new_glean() -> tempfile::TempDir {
    let dir = tempfile::tempdir().unwrap();
    let tmpname = dir.path().display().to_string();

    let cfg = Configuration {
        data_path: tmpname,
        application_id: GLOBAL_APPLICATION_ID.into(),
        upload_enabled: true,
        max_events: None,
        delay_ping_lifetime_io: false,
        channel: None,
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

    let dir = new_glean();

    // Register and submit a ping for testing
    let ping_type = PingType::new("test", true, /* send_if_empty */ true, vec![]);
    glean::register_ping_type(&ping_type);
    ping_type.submit(None);
    glean::dispatcher::block_on_queue();

    // Read the ping from disk.
    // We know where it should be placed.
    let pings_dir = dir.path().join("pending_pings");
    let pending_pings: Vec<PathBuf> = pings_dir
        .read_dir()
        .unwrap()
        .filter_map(|entry| entry.ok())
        .filter_map(|entry| match entry.file_type() {
            Ok(f) if f.is_file() => Some(entry.path()),
            _ => None,
        })
        .collect();

    // There should only be one: our custom ping.
    assert_eq!(1, pending_pings.len());

    // 1. line: the URL
    // 2. line: the JSON body
    let file = File::open(&pending_pings[0]).unwrap();
    let mut lines = BufReader::new(file).lines();
    let _url = lines.next().unwrap().unwrap();
    let body = lines.next().unwrap().unwrap();

    // Now validate against the vendored schema
    let data = serde_json::from_str(&body).unwrap();
    let cfg = jsonschema_valid::Config::from_schema(&schema, Some(Draft::Draft6)).unwrap();
    let validation = cfg.validate(&data);
    match validation {
        Ok(()) => {}
        Err(e) => {
            let errors = e.map(|e| format!("{}", e)).collect::<Vec<_>>();
            assert!(false, "Data: {:#?}\nErrors: {:#?}", data, errors);
        }
    }
}
