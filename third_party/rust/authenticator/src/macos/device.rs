/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate log;

use consts::{CID_BROADCAST, HID_RPT_SIZE};
use core_foundation::base::*;
use platform::iokit::*;
use std::convert::TryInto;
use std::io;
use std::io::{Read, Write};
use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::Duration;
use u2ftypes::U2FDevice;

const READ_TIMEOUT: u64 = 15;

pub struct Device {
    device_ref: IOHIDDeviceRef,
    cid: [u8; 4],
    report_rx: Receiver<Vec<u8>>,
}

impl Device {
    pub fn new(dev_info: (IOHIDDeviceRef, Receiver<Vec<u8>>)) -> io::Result<Self> {
        let (device_ref, report_rx) = dev_info;
        Ok(Self {
            device_ref,
            cid: CID_BROADCAST,
            report_rx,
        })
    }

    pub fn is_u2f(&self) -> bool {
        true
    }
}

impl PartialEq for Device {
    fn eq(&self, other_device: &Device) -> bool {
        self.device_ref == other_device.device_ref
    }
}

impl Read for Device {
    fn read(&mut self, mut bytes: &mut [u8]) -> io::Result<usize> {
        let timeout = Duration::from_secs(READ_TIMEOUT);
        let data = match self.report_rx.recv_timeout(timeout) {
            Ok(v) => v,
            Err(e) if e == RecvTimeoutError::Timeout => {
                return Err(io::Error::new(io::ErrorKind::TimedOut, e));
            }
            Err(e) => {
                return Err(io::Error::new(io::ErrorKind::UnexpectedEof, e));
            }
        };
        bytes.write(&data)
    }
}

impl Write for Device {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        assert_eq!(bytes.len(), HID_RPT_SIZE + 1);

        let report_id = i64::from(bytes[0]);
        // Skip report number when not using numbered reports.
        let start = if report_id == 0x0 { 1 } else { 0 };
        let data = &bytes[start..];

        let result = unsafe {
            IOHIDDeviceSetReport(
                self.device_ref,
                kIOHIDReportTypeOutput,
                report_id.try_into().unwrap(),
                data.as_ptr(),
                data.len() as CFIndex,
            )
        };
        if result != 0 {
            warn!("set_report sending failure = {0:X}", result);
            return Err(io::Error::from_raw_os_error(result));
        }
        trace!("set_report sending success = {0:X}", result);

        Ok(bytes.len())
    }

    // USB HID writes don't buffer, so this will be a nop.
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl U2FDevice for Device {
    fn get_cid(&self) -> &[u8; 4] {
        &self.cid
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        self.cid = cid;
    }
}
