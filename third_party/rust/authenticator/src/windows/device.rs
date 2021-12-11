/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs::{File, OpenOptions};
use std::io;
use std::io::{Read, Write};
use std::os::windows::io::AsRawHandle;

use super::winapi::DeviceCapabilities;
use crate::consts::{CID_BROADCAST, FIDO_USAGE_PAGE, FIDO_USAGE_U2FHID, MAX_HID_RPT_SIZE};
use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};

#[derive(Debug)]
pub struct Device {
    path: String,
    file: File,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
}

impl Device {
    pub fn new(path: String) -> io::Result<Self> {
        let file = OpenOptions::new().read(true).write(true).open(&path)?;
        Ok(Self {
            path,
            file,
            cid: CID_BROADCAST,
            dev_info: None,
        })
    }

    pub fn is_u2f(&self) -> bool {
        match DeviceCapabilities::new(self.file.as_raw_handle()) {
            Ok(caps) => caps.usage() == FIDO_USAGE_U2FHID && caps.usage_page() == FIDO_USAGE_PAGE,
            _ => false,
        }
    }
}

impl PartialEq for Device {
    fn eq(&self, other: &Device) -> bool {
        self.path == other.path
    }
}

impl Read for Device {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        // Windows always includes the report ID.
        let mut input = [0u8; MAX_HID_RPT_SIZE + 1];
        let _ = self.file.read(&mut input)?;
        bytes.clone_from_slice(&input[1..]);
        Ok(bytes.len() as usize)
    }
}

impl Write for Device {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        self.file.write(bytes)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.file.flush()
    }
}

impl U2FDevice for Device {
    fn get_cid<'a>(&'a self) -> &'a [u8; 4] {
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

    fn get_property(&self, _prop_name: &str) -> io::Result<String> {
        Err(io::Error::new(io::ErrorKind::Other, "Not implemented"))
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
