/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};
use std::io;
use std::io::{Read, Write};

pub struct Device {}

impl Device {
    pub fn new(path: String) -> io::Result<Self> {
        panic!("not implemented");
    }

    pub fn is_u2f(&self) -> bool {
        panic!("not implemented");
    }
}

impl Read for Device {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        panic!("not implemented");
    }
}

impl Write for Device {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        panic!("not implemented");
    }

    fn flush(&mut self) -> io::Result<()> {
        panic!("not implemented");
    }
}

impl U2FDevice for Device {
    fn get_cid<'a>(&'a self) -> &'a [u8; 4] {
        panic!("not implemented");
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        panic!("not implemented");
    }

    fn in_rpt_size(&self) -> usize {
        panic!("not implemented");
    }

    fn out_rpt_size(&self) -> usize {
        panic!("not implemented");
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String> {
        panic!("not implemented")
    }

    fn get_device_info(&self) -> U2FDeviceInfo {
        panic!("not implemented")
    }

    fn set_device_info(&mut self, dev_info: U2FDeviceInfo) {
        panic!("not implemented")
    }
}
