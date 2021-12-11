/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate log;

use crate::consts::{CID_BROADCAST, MAX_HID_RPT_SIZE};
use crate::platform::iokit::*;
use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};
use core_foundation::base::*;
use core_foundation::string::*;
use std::convert::TryInto;
use std::io;
use std::io::{Read, Write};
use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::Duration;

const READ_TIMEOUT: u64 = 15;

pub struct Device {
    device_ref: IOHIDDeviceRef,
    cid: [u8; 4],
    report_rx: Receiver<Vec<u8>>,
    dev_info: Option<U2FDeviceInfo>,
}

impl Device {
    pub fn new(dev_ids: (IOHIDDeviceRef, Receiver<Vec<u8>>)) -> io::Result<Self> {
        let (device_ref, report_rx) = dev_ids;
        Ok(Self {
            device_ref,
            cid: CID_BROADCAST,
            report_rx,
            dev_info: None,
        })
    }

    pub fn is_u2f(&self) -> bool {
        true
    }

    unsafe fn get_property_macos(&self, prop_name: &str) -> io::Result<String> {
        let prop_ref = IOHIDDeviceGetProperty(
            self.device_ref,
            CFString::new(prop_name).as_concrete_TypeRef(),
        );
        if prop_ref.is_null() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!(
                    "IOHIDDeviceGetProperty received nullptr for property {}",
                    prop_name
                ),
            ));
        }

        if CFGetTypeID(prop_ref) != CFStringGetTypeID() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!(
                    "IOHIDDeviceGetProperty returned non-string type for property {}",
                    prop_name
                ),
            ));
        }

        Ok(CFString::from_void(prop_ref).to_string())
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
        assert_eq!(bytes.len(), self.out_rpt_size() + 1);

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

    fn in_rpt_size(&self) -> usize {
        MAX_HID_RPT_SIZE
    }

    fn out_rpt_size(&self) -> usize {
        MAX_HID_RPT_SIZE
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String> {
        unsafe { self.get_property_macos(prop_name) }
    }

    fn get_device_info(&self) -> U2FDeviceInfo {
        // unwrap is okay, as dev_info must have already been set, else
        // a programmer error
        self.dev_info.clone().unwrap()
    }

    fn set_device_info(&mut self, dev_info: U2FDeviceInfo) {
        self.dev_info = Some(dev_info);
    }
}
