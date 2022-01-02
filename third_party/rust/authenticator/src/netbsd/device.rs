/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;
use std::io::Read;
use std::io::Write;
use std::mem;

use crate::consts::CID_BROADCAST;
use crate::consts::MAX_HID_RPT_SIZE;
use crate::platform::fd::Fd;
use crate::platform::uhid;
use crate::u2ftypes::{U2FDevice, U2FDeviceInfo};
use crate::util::io_err;

#[derive(Debug)]
pub struct Device {
    fd: Fd,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
}

impl Device {
    pub fn new(fd: Fd) -> io::Result<Self> {
        Ok(Self {
            fd,
            cid: CID_BROADCAST,
            dev_info: None,
        })
    }

    pub fn is_u2f(&mut self) -> bool {
        if !uhid::is_u2f_device(&self.fd) {
            return false;
        }
        // This step is not strictly necessary -- NetBSD puts fido
        // devices into raw mode automatically by default, but in
        // principle that might change, and this serves as a test to
        // verify that we're running on a kernel with support for raw
        // mode at all so we don't get confused issuing writes that try
        // to set the report descriptor rather than transfer data on
        // the output interrupt pipe as we need.
        match uhid::hid_set_raw(&self.fd, true) {
            Ok(_) => (),
            Err(_) => return false,
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
            let mut pfd: libc::pollfd = unsafe { mem::zeroed() };
            pfd.fd = self.fd.fileno;
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

impl PartialEq for Device {
    fn eq(&self, other: &Device) -> bool {
        self.fd == other.fd
    }
}

impl Read for Device {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let bufp = buf.as_mut_ptr() as *mut libc::c_void;
        let nread = unsafe { libc::read(self.fd.fileno, bufp, buf.len()) };
        if nread == -1 {
            return Err(io::Error::last_os_error());
        }
        Ok(nread as usize)
    }
}

impl Write for Device {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        // Always skip the first byte (report number)
        let data = &buf[1..];
        let data_ptr = data.as_ptr() as *const libc::c_void;
        let nwrit = unsafe { libc::write(self.fd.fileno, data_ptr, data.len()) };
        if nwrit == -1 {
            return Err(io::Error::last_os_error());
        }
        // Pretend we wrote the report number byte
        Ok(nwrit as usize + 1)
    }

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
