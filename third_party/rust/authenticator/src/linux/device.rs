/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::ffi::{CString, OsString};
use std::io;
use std::io::{Read, Write};
use std::os::unix::prelude::*;

use crate::consts::CID_BROADCAST;
use crate::platform::{hidraw, monitor};
use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};
use crate::util::from_unix_result;

#[derive(Debug)]
pub struct Device {
    path: OsString,
    fd: libc::c_int,
    in_rpt_size: usize,
    out_rpt_size: usize,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
}

impl Device {
    pub fn new(path: OsString) -> io::Result<Self> {
        let cstr = CString::new(path.as_bytes())?;
        let fd = unsafe { libc::open(cstr.as_ptr(), libc::O_RDWR) };
        let fd = from_unix_result(fd)?;
        let (in_rpt_size, out_rpt_size) = hidraw::read_hid_rpt_sizes_or_defaults(fd);
        Ok(Self {
            path,
            fd,
            in_rpt_size,
            out_rpt_size,
            cid: CID_BROADCAST,
            dev_info: None,
        })
    }

    pub fn is_u2f(&self) -> bool {
        hidraw::is_u2f_device(self.fd)
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        // Close the fd, ignore any errors.
        let _ = unsafe { libc::close(self.fd) };
    }
}

impl PartialEq for Device {
    fn eq(&self, other: &Device) -> bool {
        self.path == other.path
    }
}

impl Read for Device {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let bufp = buf.as_mut_ptr() as *mut libc::c_void;
        let rv = unsafe { libc::read(self.fd, bufp, buf.len()) };
        from_unix_result(rv as usize)
    }
}

impl Write for Device {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let bufp = buf.as_ptr() as *const libc::c_void;
        let rv = unsafe { libc::write(self.fd, bufp, buf.len()) };
        from_unix_result(rv as usize)
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
        self.in_rpt_size
    }

    fn out_rpt_size(&self) -> usize {
        self.out_rpt_size
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String> {
        monitor::get_property_linux(&self.path, prop_name)
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
