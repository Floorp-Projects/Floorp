/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::winapi::DeviceCapabilities;
use crate::consts::{
    Capability, CID_BROADCAST, FIDO_USAGE_PAGE, FIDO_USAGE_U2FHID, MAX_HID_RPT_SIZE,
};
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::transport::hid::HIDDevice;
use crate::transport::{FidoDevice, FidoProtocol, HIDError, SharedSecret};
use crate::u2ftypes::U2FDeviceInfo;
use std::fs::{File, OpenOptions};
use std::hash::{Hash, Hasher};
use std::io::{self, Read, Write};
use std::os::windows::io::AsRawHandle;

#[derive(Debug)]
pub struct Device {
    path: String,
    file: File,
    cid: [u8; 4],
    dev_info: Option<U2FDeviceInfo>,
    secret: Option<SharedSecret>,
    authenticator_info: Option<AuthenticatorInfo>,
    protocol: FidoProtocol,
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
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        // Windows always includes the report ID.
        let mut input = [0u8; MAX_HID_RPT_SIZE + 1];
        let _ = self.file.read(&mut input)?;
        bytes.clone_from_slice(&input[1..]);
        Ok(bytes.len())
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

impl HIDDevice for Device {
    type BuildParameters = String;
    type Id = String;

    fn new(path: String) -> Result<Self, (HIDError, Self::Id)> {
        debug!("Opening device {:?}", path);
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .open(&path)
            .map_err(|e| (HIDError::IO(Some(path.clone().into()), e), path.clone()))?;
        let mut res = Self {
            path,
            file,
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
            Err((HIDError::DeviceNotSupported, res.path))
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
        match DeviceCapabilities::new(self.file.as_raw_handle()) {
            Ok(caps) => caps.usage() == FIDO_USAGE_U2FHID && caps.usage_page() == FIDO_USAGE_PAGE,
            _ => false,
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
