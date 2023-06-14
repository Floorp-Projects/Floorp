/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use crate::consts::{Capability, CID_BROADCAST, MAX_HID_RPT_SIZE};
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::transport::hid::HIDDevice;
use crate::transport::platform::uhid;
use crate::transport::{FidoDevice, FidoProtocol, HIDError, SharedSecret};
use crate::u2ftypes::U2FDeviceInfo;
use crate::util::from_unix_result;
use crate::util::io_err;
use std::ffi::{CString, OsString};
use std::hash::{Hash, Hasher};
use std::io::{self, Read, Write};
use std::mem;
use std::os::unix::prelude::*;

#[derive(Debug)]
pub struct Device {
    path: OsString,
    fd: libc::c_int,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
    secret: Option<SharedSecret>,
    authenticator_info: Option<AuthenticatorInfo>,
    protocol: FidoProtocol,
}

impl Device {
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

            if self.write(&buf)? != buf.len() {
                return Err(io_err("write ping failed"));
            }

            // Wait for response
            let mut pfd: libc::pollfd = unsafe { mem::zeroed() };
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

            // Read response.  When reports come in they are all
            // exactly the same size, with no report id byte because
            // there is only one report.
            let n = self.read(&mut buf[1..])?;
            if n != buf.len() - 1 {
                return Err(io_err("read pong failed"));
            }

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

impl Eq for Device {}

impl Hash for Device {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.path.hash(state);
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

impl HIDDevice for Device {
    type BuildParameters = OsString;
    type Id = OsString;

    fn new(path: OsString) -> Result<Self, (HIDError, Self::Id)> {
        let cstr =
            CString::new(path.as_bytes()).map_err(|_| (HIDError::DeviceError, path.clone()))?;
        let fd = unsafe { libc::open(cstr.as_ptr(), libc::O_RDWR) };
        let fd = from_unix_result(fd).map_err(|e| (e.into(), path.clone()))?;
        let mut res = Self {
            path,
            fd,
            cid: CID_BROADCAST,
            dev_info: None,
            secret: None,
            authenticator_info: None,
            protocol: FidoProtocol::CTAP2,
        };
        if res.is_u2f() {
            info!("new device {:?}", res.path);
            Ok(res)
        } else {
            Err((HIDError::DeviceNotSupported, res.path.clone()))
        }
    }

    fn id(&self) -> Self::Id {
        self.path.clone()
    }

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

impl FidoDevice for Device {
    fn pre_init(&mut self) -> Result<(), HIDError> {
        HIDDevice::pre_init(self)
    }

    fn should_try_ctap2(&self) -> bool {
        HIDDevice::get_device_info(self)
            .cap_flags
            .contains(Capability::CBOR)
    }

    fn initialized(&self) -> bool {
        // During successful init, the broadcast channel id gets repplaced by an actual one
        self.cid != CID_BROADCAST
    }

    fn is_u2f(&mut self) -> bool {
        if !uhid::is_u2f_device(self.fd) {
            return false;
        }
        if self.ping().is_err() {
            return false;
        }
        true
    }

    fn get_shared_secret(&self) -> Option<&SharedSecret> {
        self.secret.as_ref()
    }

    fn set_shared_secret(&mut self, secret: SharedSecret) {
        self.secret = Some(secret);
    }

    fn get_authenticator_info(&self) -> Option<&AuthenticatorInfo> {
        self.authenticator_info.as_ref()
    }

    fn set_authenticator_info(&mut self, authenticator_info: AuthenticatorInfo) {
        self.authenticator_info = Some(authenticator_info);
    }

    fn get_protocol(&self) -> FidoProtocol {
        self.protocol
    }

    fn downgrade_to_ctap1(&mut self) {
        self.protocol = FidoProtocol::CTAP1;
    }
}
