/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::ffi::{CString, OsString};
use std::io;
use std::io::{Read, Write};
use std::os::unix::prelude::*;

use crate::consts::{CID_BROADCAST, MAX_HID_RPT_SIZE};
use crate::transport::platform::uhid;
use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};
use crate::util::from_unix_result;
use crate::util::io_err;

#[derive(Debug)]
pub struct Device {
    path: OsString,
    fd: libc::c_int,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
}

impl Device {
    pub fn new(path: OsString) -> io::Result<Self> {
        let cstr = CString::new(path.as_bytes())?;
        let fd = unsafe { libc::open(cstr.as_ptr(), libc::O_RDWR) };
        let fd = from_unix_result(fd)?;
        Ok(Self {
            path,
            fd,
            cid: CID_BROADCAST,
            dev_info: None,
        })
    }

    pub fn is_u2f(&mut self) -> bool {
        if !uhid::is_u2f_device(self.fd) {
            return false;
        }
        if let Err(_) = self.ping() {
            return false;
        }
        true
    }

    fn ping(&mut self) -> io::Result<()> {
        for i in 0..10 {
            let mut buf = vec![0u8; 1 + MAX_HID_RPT_SIZE];

            buf[0] = 0; // report number
            buf[1] = 0xff; // CID_BROADCAST
            buf[2] = 0xff;
            buf[3] = 0xff;
            buf[4] = 0xff;
            buf[5] = 0x81; // ping
            buf[6] = 0;
            buf[7] = 1; // one byte

            self.write(&buf[..])?;

            // Wait for response
            let mut pfd: libc::pollfd = unsafe { std::mem::zeroed() };
            pfd.fd = self.fd;
            pfd.events = libc::POLLIN;
            let nfds = unsafe { libc::poll(&mut pfd, 1, 100) };
            if nfds == -1 {
                return Err(io::Error::last_os_error());
            }
            if nfds == 0 {
                debug!("device timeout {}", i);
                continue;
            }

            // Read response
            self.read(&mut buf[..])?;

            return Ok(());
        }

        Err(io_err("no response from device"))
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
        let report_id = buf[0] as i64;
        // Skip report number when not using numbered reports.
        let start = if report_id == 0x0 { 1 } else { 0 };
        let data = &buf[start..];

        let data_ptr = data.as_ptr() as *const libc::c_void;
        let rv = unsafe { libc::write(self.fd, data_ptr, data.len()) };
        from_unix_result(rv as usize + 1)
    }

    // USB HID writes don't buffer, so this will be a nop.
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
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
