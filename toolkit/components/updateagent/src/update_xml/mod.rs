/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

mod download;
mod parse;
mod update_url;

use log::{debug, info};
pub use parse::{parse_file, Update, UpdatePatchType, UpdateType, UpdateXmlPatch};
use std::ffi::OsString;

pub fn sync_download(
    download_dir: OsString,
    save_path: OsString,
    job_name: OsString,
) -> Result<(), String> {
    let url = OsString::from(update_url::generate()?);
    info!(
        "Checking for updates using URL: {:?}.\nSaving to {:?}",
        url, save_path
    );
    download::sync_download_url(download_dir, save_path, job_name, url)?;
    debug!("XML download successful");
    Ok(())
}
