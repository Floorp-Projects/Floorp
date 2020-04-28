/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::{env::current_exe, ffi::OsString, path::PathBuf};
use winapi::shared::{
    minwindef::MAX_PATH,
    winerror::{FAILED, HRESULT},
};
use wio::wide::{FromWide, ToWide};

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

#[link(name = "updatecommon", kind = "static")]
extern "C" {
    fn get_common_update_directory(install_path: *const u16, result: *mut u16) -> HRESULT;
}
pub fn get_update_directory() -> Result<OsString, String> {
    let mut binary_path = current_exe()
        .map_err(|e| format!("Couldn't get executing binary's path: {}", e))?
        .parent()
        .ok_or("Couldn't get executing binary's parent directory")?
        .as_os_str()
        .to_wide_null();
    // It looks like Path.as_os_str returns a path without a trailing slash, so this may be
    // unnecessary. But the documentation does not appear to guarantee the lack of trailing slash.
    // get_common_update_directory, however, must be given the installation path without a trailing
    // slash. So remove any trailing path slashes, just to be safe.
    strip_trailing_path_separators(&mut binary_path);

    let mut buffer: Vec<u16> = Vec::with_capacity(MAX_PATH + 1);
    let hr = unsafe { get_common_update_directory(binary_path.as_ptr(), buffer.as_mut_ptr()) };
    if FAILED(hr) {
        return Err(format!("Failed to get update directory: HRESULT={}", hr));
    }
    Ok(unsafe { OsString::from_wide_ptr_null(buffer.as_ptr()) })
}

fn strip_trailing_path_separators(path: &mut Vec<u16>) {
    let forward_slash = '/' as u16;
    let backslash = '\\' as u16;
    for character in path.iter_mut().rev() {
        if *character == forward_slash || *character == backslash {
            *character = 0;
        } else if *character != 0 {
            break;
        }
    }
}
