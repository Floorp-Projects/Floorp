/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[cfg(target_os = "windows")]
type ProcessHandle = winapi::um::winnt::HANDLE;

pub struct ProcessReader {
    process: ProcessHandle,
}

mod error;
mod process_reader;
