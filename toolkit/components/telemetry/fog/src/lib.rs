/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use glean_preview::Configuration;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::nsAString;

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

  NS_OK
}
