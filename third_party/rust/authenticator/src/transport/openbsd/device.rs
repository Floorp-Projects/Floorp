/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
use crate::consts::{Capability, CID_BROADCAST, MAX_HID_RPT_SIZE};
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::transport::hid::HIDDevice;
use crate::transport::platform::monitor::WrappedOpenDevice;
use crate::transport::{FidoDevice, FidoProtocol, HIDError, SharedSecret};
use crate::u2ftypes::U2FDeviceInfo;
use crate::util::{from_unix_result, io_err};
use std::ffi::{CString, OsString};
use std::hash::{Hash, Hasher};
use std::io::{self, Read, Write};
use std::mem;
use std::os::unix::ffi::OsStrExt;

#[derive(Debug)]
pub struct Device {
    path: OsString,
    fd: libc::c_int,
    in_rpt_size: usize,
    out_rpt_size: usize,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
    secret: Option<SharedSecret>,
    authenticator_info: Option<AuthenticatorInfo>,
    protocol: FidoProtocol,
}

impl Device {
    fn ping(&mut self) -> io::Result<()> {
        let capacity = 256;

        for _ in 0..10 {
            let mut data = vec![0u8; capacity];

            // Send 1 byte ping
            // self.write_all requires Device to be mut. This can't be done at the moment,
            // and this is a workaround anyways, so writing by hand instead.
            self.write_all(&[0, 0xff, 0xff, 0xff, 0xff, 0x81, 0, 1])?;

            // Wait for response
            let mut pfd: libc::pollfd = unsafe { mem::zeroed() };
            pfd.fd = self.fd;
            pfd.events = libc::POLLIN;
            if from_unix_result(unsafe { libc::poll(&mut pfd, 1, 100) })? == 0 {
                debug!("device {:?} timeout", self.path);
                continue;
            }

            // Read response
            self.read(&mut data[..])?;

            return Ok(());
        }

        Err(io_err("no response from device"))
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        // Close the fd, ignore any errors.
        let _ = unsafe { libc::close(self.fd) };
        debug!("device {:?} closed", self.path);
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
        // The path should be the only identifying member for a device
        // If the path is the same, its the same device
        self.path.hash(state);
    }
}

impl Read for Device {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let buf_ptr = buf.as_mut_ptr() as *mut libc::c_void;
        let rv = unsafe { libc::read(self.fd, buf_ptr, buf.len()) };
        from_unix_result(rv as usize)
    }
}

impl Write for Device {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        // Always skip the first byte (report number)
        let data = &buf[1..];
        let data_ptr = data.as_ptr() as *const libc::c_void;
        let rv = unsafe { libc::write(self.fd, data_ptr, data.len()) };
        Ok(from_unix_result(rv as usize)? + 1)
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl HIDDevice for Device {
    type BuildParameters = WrappedOpenDevice;
    type Id = OsString;

    fn new(fido: WrappedOpenDevice) -> Result<Self, (HIDError, Self::Id)> {
        debug!("device found: {:?}", fido);
        let mut res = Self {
            path: fido.os_path,
            fd: fido.fd,
            in_rpt_size: MAX_HID_RPT_SIZE,
            out_rpt_size: MAX_HID_RPT_SIZE,
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
        self.in_rpt_size
    }

    fn out_rpt_size(&self) -> usize {
        self.out_rpt_size
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
        debug!("device {:?} is U2F/FIDO", self.path);

        // From OpenBSD's libfido2 in 6.6-current:
        // "OpenBSD (as of 201910) has a bug that causes it to lose
        // track of the DATA0/DATA1 sequence toggle across uhid device
        // open and close. This is a terrible hack to work around it."
        match self.ping() {
            Ok(_) => true,
            Err(err) => {
                debug!("device {:?} is not responding: {}", self.path, err);
                false
            }
        }
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
