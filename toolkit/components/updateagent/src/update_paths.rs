/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::{ffi::OsString, path::PathBuf};

pub fn get_download_xml() -> Result<OsString, String> {
    let mut update_dir = PathBuf::from(get_update_directory()?);
    update_dir.push("download.xml");
    Ok(update_dir.into_os_string())
}

pub fn get_install_hash() -> Result<OsString, String> {
    let update_dir = PathBuf::from(get_update_directory()?);
    update_dir
        .file_name()
        .map(|file_name| OsString::from(file_name))
        .ok_or("Couldn't get update directory name".to_string())
}

pub fn get_update_directory() -> Result<OsString, String> {
    // TODO: Get the real update directory
    Ok(OsString::from("C:\\test\\"))
}
