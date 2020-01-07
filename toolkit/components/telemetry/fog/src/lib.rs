/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use glean_preview::Configuration;
use glean_preview::metrics::PingType;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::nsAString;
use std::{thread, time};
use std::io::Error;
use std::thread::JoinHandle;

#[no_mangle]
pub unsafe extern "C" fn fog_init(upload_enabled: bool, data_dir: &nsAString) -> nsresult {
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

  // We ignore the returned JoinHandle for the nonce.
  // The detached thread will live until this process (the main process) dies.
  if prototype_ping_init().is_err() {
    return NS_ERROR_FAILURE
  }

  NS_OK
}

fn prototype_ping_init() -> Result<JoinHandle<()>, Error> {
  thread::Builder::new().name("fogotype_ping".to_owned()).spawn(|| {
    let prototype_ping = PingType::new("prototype", true, true);
    glean_preview::register_ping_type(&prototype_ping);

    let an_hour = time::Duration::from_secs(60 * 60);
    loop {
      thread::sleep(an_hour);
      prototype_ping.send();
    }
  })
}
