/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::transport::hid::HIDDevice;
use crate::transport::{FidoDevice, FidoProtocol};
use crate::transport::{HIDError, SharedSecret};
use crate::u2ftypes::U2FDeviceInfo;
use std::hash::Hash;
use std::io;
use std::io::{Read, Write};
use std::path::PathBuf;

#[derive(Debug, Hash, PartialEq, Eq)]
pub struct Device {}

impl Read for Device {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        unimplemented!();
    }
}

impl Write for Device {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        unimplemented!();
    }

    fn flush(&mut self) -> io::Result<()> {
        unimplemented!();
    }
}

impl HIDDevice for Device {
    type BuildParameters = PathBuf;
    type Id = PathBuf;

    fn new(parameters: Self::BuildParameters) -> Result<Self, (HIDError, Self::Id)> {
        unimplemented!();
    }

    fn id(&self) -> Self::Id {
        unimplemented!()
    }

    fn get_cid(&self) -> &[u8; 4] {
        unimplemented!();
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        unimplemented!();
    }

    fn in_rpt_size(&self) -> usize {
        unimplemented!();
    }

    fn out_rpt_size(&self) -> usize {
        unimplemented!();
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String> {
        unimplemented!();
    }

    fn get_device_info(&self) -> U2FDeviceInfo {
        unimplemented!();
    }

    fn set_device_info(&mut self, dev_info: U2FDeviceInfo) {
        unimplemented!();
    }
}

impl FidoDevice for Device {
    fn pre_init(&mut self) -> Result<(), HIDError> {
        unimplemented!();
    }

    fn should_try_ctap2(&self) -> bool {
        unimplemented!();
    }

    fn initialized(&self) -> bool {
        unimplemented!();
    }

    fn is_u2f(&mut self) -> bool {
        unimplemented!()
    }

    fn get_authenticator_info(&self) -> Option<&AuthenticatorInfo> {
        unimplemented!()
    }

    fn set_authenticator_info(&mut self, authenticator_info: AuthenticatorInfo) {
        unimplemented!()
    }

    fn set_shared_secret(&mut self, secret: SharedSecret) {
        unimplemented!()
    }

    fn get_shared_secret(&self) -> Option<&SharedSecret> {
        unimplemented!()
    }

    fn get_protocol(&self) -> FidoProtocol {
        unimplemented!()
    }

    fn downgrade_to_ctap1(&mut self) {
        unimplemented!()
    }
}
