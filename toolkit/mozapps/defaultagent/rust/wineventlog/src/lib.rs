/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Very simple implementation of logging via the Windows Event Log

use std::ffi::OsStr;
use std::ptr;

use log::{Level, Metadata, Record};
use winapi::shared::minwindef::WORD;
use winapi::um::{winbase, winnt};
use wio::wide::ToWide;

pub struct EventLogger {
    pub name: Vec<u16>,
}

impl EventLogger {
    pub fn new(name: impl AsRef<OsStr>) -> Self {
        EventLogger {
            name: name.to_wide_null(),
        }
    }
}

impl log::Log for EventLogger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= log::max_level()
    }

    fn log(&self, record: &Record) {
        if !self.enabled(record.metadata()) {
            return;
        }

        let name = self.name.as_ptr();
        let msg = format!("{} - {}", record.level(), record.args()).to_wide_null();

        // Open and close the event log handle on every message, for simplicity.
        let event_log;
        unsafe {
            event_log = winbase::RegisterEventSourceW(ptr::null(), name);
            if event_log.is_null() {
                return;
            }
        }

        let level = match record.level() {
            Level::Error => winnt::EVENTLOG_ERROR_TYPE,
            Level::Warn => winnt::EVENTLOG_WARNING_TYPE,
            Level::Info | Level::Debug | Level::Trace => winnt::EVENTLOG_INFORMATION_TYPE,
        };

        unsafe {
            // mut only to match the LPCWSTR* signature
            let mut msg_array: [*const u16; 1] = [msg.as_ptr()];

            let _ = winbase::ReportEventW(
                event_log,
                level,
                0,                       // no category
                0,                       // event id 0
                ptr::null_mut(),         // no user sid
                msg_array.len() as WORD, // string count
                0,                       // 0 bytes raw data
                msg_array.as_mut_ptr(),  // strings
                ptr::null_mut(),         // no raw data
            );

            let _ = winbase::DeregisterEventSource(event_log);
        }
    }

    fn flush(&self) {}
}
