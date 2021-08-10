// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fs::File;
use std::io::Write;
use std::sync::RwLock;

#[cfg(target_os = "windows")]
use std::os::windows::io::FromRawHandle;

#[cfg(target_os = "windows")]
use std::ffi::c_void;

#[cfg(not(target_os = "windows"))]
use std::os::unix::io::FromRawFd;

use serde::Serialize;

/// An implementation of log::Log that writes log messages in JSON format to a
/// file descriptor/handle. The logging level is ignored in this implementation:
/// it is up to the receiver of these log messages (on the language binding
/// side) to filter the log messages based on their level.
/// The JSON payload of each message in an object with the following keys:
///    - `level` (string): One of the logging levels defined here:
///      https://docs.rs/log/0.4.11/log/enum.Level.html
///    - `message` (string): The logging message.
pub struct FdLogger {
    pub file: RwLock<File>,
}

#[derive(Serialize)]
struct FdLoggingRecord {
    level: String,
    message: String,
}

#[cfg(target_os = "windows")]
unsafe fn get_file_from_fd(fd: u64) -> File {
    File::from_raw_handle(fd as *mut c_void)
}

#[cfg(not(target_os = "windows"))]
unsafe fn get_file_from_fd(fd: u64) -> File {
    File::from_raw_fd(fd as i32)
}

impl FdLogger {
    pub unsafe fn new(fd: u64) -> Self {
        FdLogger {
            file: RwLock::new(get_file_from_fd(fd)),
        }
    }
}

impl log::Log for FdLogger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        // This logger always emits logging messages of any level, and the
        // language binding consuming these messages is responsible for
        // filtering and routing them.
        true
    }

    fn log(&self, record: &log::Record) {
        // Normally, classes implementing the Log trait would filter based on
        // the log level here. But in this case, we want to emit all log
        // messages and let the logging system in the language binding filter
        // and route them.
        let payload = FdLoggingRecord {
            level: record.level().to_string(),
            message: record.args().to_string(),
        };
        let _ = writeln!(
            self.file.write().unwrap(),
            "{}",
            serde_json::to_string(&payload).unwrap()
        );
    }

    fn flush(&self) {
        let _ = self.file.write().unwrap().flush();
    }
}
