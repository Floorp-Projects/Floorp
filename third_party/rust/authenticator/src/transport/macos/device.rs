/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate log;

use crate::consts::{Capability, CID_BROADCAST, MAX_HID_RPT_SIZE};
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::transport::hid::HIDDevice;
use crate::transport::platform::iokit::*;
use crate::transport::{FidoDevice, FidoProtocol, HIDError, SharedSecret};
use crate::u2ftypes::U2FDeviceInfo;
use core_foundation::base::*;
use core_foundation::string::*;
use std::convert::TryInto;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::io;
use std::io::{Read, Write};
use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::Duration;

const READ_TIMEOUT: u64 = 15;

pub struct Device {
    device_ref: IOHIDDeviceRef,
    cid: [u8; 4],
    report_rx: Option<Receiver<Vec<u8>>>,
    dev_info: Option<U2FDeviceInfo>,
    secret: Option<SharedSecret>,
    authenticator_info: Option<AuthenticatorInfo>,
    protocol: FidoProtocol,
}

impl Device {
    unsafe fn get_property_macos(&self, prop_name: &str) -> io::Result<String> {
        let prop_ref = IOHIDDeviceGetProperty(
            self.device_ref,
            CFString::new(prop_name).as_concrete_TypeRef(),
        );
        if prop_ref.is_null() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("IOHIDDeviceGetProperty received nullptr for property {prop_name}"),
            ));
        }

        if CFGetTypeID(prop_ref) != CFStringGetTypeID() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("IOHIDDeviceGetProperty returned non-string type for property {prop_name}"),
            ));
        }

        Ok(CFString::from_void(prop_ref).to_string())
    }
}

impl fmt::Debug for Device {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Device").field("cid", &self.cid).finish()
    }
}

impl PartialEq for Device {
    fn eq(&self, other_device: &Device) -> bool {
        self.device_ref == other_device.device_ref
    }
}

impl Eq for Device {}

impl Hash for Device {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // The path should be the only identifying member for a device
        // If the path is the same, its the same device
        self.device_ref.hash(state);
    }
}

impl Read for Device {
    fn read(&mut self, mut bytes: &mut [u8]) -> io::Result<usize> {
        if let Some(rx) = &self.report_rx {
            let timeout = Duration::from_secs(READ_TIMEOUT);
            let data = match rx.recv_timeout(timeout) {
                Ok(v) => v,
                Err(e) if e == RecvTimeoutError::Timeout => {
                    return Err(io::Error::new(io::ErrorKind::TimedOut, e));
                }
                Err(e) => {
                    return Err(io::Error::new(io::ErrorKind::UnexpectedEof, e));
                }
            };
            bytes.write(&data)
        } else {
            Err(io::Error::from(io::ErrorKind::Unsupported))
        }
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

impl HIDDevice for Device {
    type BuildParameters = (IOHIDDeviceRef, Receiver<Vec<u8>>);
    type Id = IOHIDDeviceRef;

    fn new(dev_ids: Self::BuildParameters) -> Result<Self, (HIDError, Self::Id)> {
        let (device_ref, report_rx) = dev_ids;
        Ok(Self {
            device_ref,
            cid: CID_BROADCAST,
            report_rx: Some(report_rx),
            dev_info: None,
            secret: None,
            authenticator_info: None,
            protocol: FidoProtocol::CTAP2,
        })
    }

    fn id(&self) -> Self::Id {
        self.device_ref
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
        self.cid != CID_BROADCAST
    }
    fn is_u2f(&mut self) -> bool {
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
