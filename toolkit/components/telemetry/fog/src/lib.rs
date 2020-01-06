/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use glean_preview::Configuration;
use glean_preview::metrics::PingType;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::nsAString;
use std::{thread, time};
use std::fs::{self, File};
use std::io::{self, BufRead, BufReader, Error, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread::JoinHandle;

#[no_mangle]
pub unsafe extern "C" fn fog_init(data_dir: &nsAString, pingsender_path: &nsAString) -> nsresult {

  let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");

  let cfg = Configuration {
    data_path: data_dir.to_string(),
    application_id: "org.mozilla.fogotype".into(),
    upload_enabled: upload_enabled,
    max_events: None,
    delay_ping_lifetime_io: false, // We will want this eventually.
  };

  if glean_preview::initialize(cfg).is_err() {
    return NS_ERROR_FAILURE
  }

  let mut data_path = PathBuf::from(data_dir.to_string());
  data_path.push("pending_pings");
  let pingsender_path = PathBuf::from(pingsender_path.to_string());

  // We ignore the returned JoinHandle for the nonce.
  // The detached thread will live until this process (the main process) dies.
  if prototype_ping_init(data_path, pingsender_path).is_err() {
    return NS_ERROR_FAILURE
  }

  NS_OK
}

fn prototype_ping_init(ping_dir: PathBuf, pingsender_path: PathBuf) -> Result<JoinHandle<()>, Error> {
  thread::Builder::new().name("fogotype_ping".to_owned()).spawn(move || {
    let prototype_ping = PingType::new("prototype", true, true);
    glean_preview::register_ping_type(&prototype_ping);

    let an_hour = time::Duration::from_secs(60 * 60);
    loop {
      thread::sleep(an_hour);
      let upload_enabled = static_prefs::pref!("datareporting.healthreport.uploadEnabled");
      glean_preview::set_upload_enabled(upload_enabled);
      prototype_ping.send();
      if let Err(_e) = send_all_pings(&ping_dir, &pingsender_path) {
        // TODO: Do something with _e.
      }
    }
  })
}

fn send_all_pings(ping_dir: &Path, pingsender_path: &Path) -> io::Result<()> {
  assert!(ping_dir.is_dir());
  // This will be a multi-step process:
  // 1. Ensure we have an (empty) subdirectory in ping_dir called "telemetry" we can work within.
  // 2. Split the endpoint out of the glean-format ping in ping_dir, writing the rest to telemetry/.
  // 3. Invoke pingsender{.exe} for the ping in telemetry to the endpoint from the glean-format ping file we most timely ripp'd.
  // 4. Return to 2 while pings remain in ping_dir

  let telemetry_dir = ping_dir.join("telemetry");
  let _ = fs::remove_dir_all(&telemetry_dir);
  fs::create_dir(&telemetry_dir)?;

  for entry in fs::read_dir(ping_dir)? {
    let entry = entry?;
    let path = entry.path();
    if !path.is_file() {
      continue;
    }
    // Do the things.
    let file = File::open(&path)?;
    let reader = BufReader::new(file);
    let lines: Vec<String> = reader.lines().filter_map(io::Result::ok).collect();

    // Sanity check: Glean SDK ping file format is two lines.
    // First line is the path of the ingestion endpoint.
    // Second line is the payload.
    if lines.len() != 2 {
      // Doesn't look like a Glean SDK-format ping. Get rid of it.
      fs::remove_file(path)?;
      continue;
    }

    let telemetry_ping_path = telemetry_dir.join(path.file_name().unwrap());
    let mut telemetry_ping_file = File::create(&telemetry_ping_path)?;
    write!(telemetry_ping_file, "{}", lines[1])?;

    fs::remove_file(path)?;

    let mut pingsender = Command::new(pingsender_path);
    pingsender.stdout(Stdio::null())
              .stderr(Stdio::null())
              .stdin(Stdio::null());

    let _status = pingsender.arg(format!("https://incoming.telemetry.mozilla.org{}", lines[0]))
                            .arg(&telemetry_ping_path)
                            .status();
  }
  Ok(())
}
