/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use crate::consts::Capability;
use crate::{RegisterResult, SignResult};

pub struct SoftwareU2FToken {}

// This is simply for platforms that aren't using the U2F Token, usually for builds
// without --feature webdriver
#[allow(dead_code)]

impl SoftwareU2FToken {
    pub fn new() -> SoftwareU2FToken {
        Self {}
    }

    pub fn register(
        &self,
        _flags: crate::RegisterFlags,
        _timeout: u64,
        _challenge: Vec<u8>,
        _application: crate::AppId,
        _key_handles: Vec<crate::KeyHandle>,
    ) -> crate::Result<crate::RegisterResult> {
        Ok(RegisterResult::CTAP1(vec![0u8; 16], self.dev_info()))
    }

    /// The implementation of this method must return quickly and should
    /// report its status via the status and callback methods
    pub fn sign(
        &self,
        _flags: crate::SignFlags,
        _timeout: u64,
        _challenge: Vec<u8>,
        _app_ids: Vec<crate::AppId>,
        _key_handles: Vec<crate::KeyHandle>,
    ) -> crate::Result<crate::SignResult> {
        Ok(SignResult::CTAP1(
            vec![0u8; 0],
            vec![0u8; 0],
            vec![0u8; 0],
            self.dev_info(),
        ))
    }

    pub fn dev_info(&self) -> crate::u2ftypes::U2FDeviceInfo {
        crate::u2ftypes::U2FDeviceInfo {
            vendor_name: b"Mozilla".to_vec(),
            device_name: b"Authenticator Webdriver Token".to_vec(),
            version_interface: 0,
            version_major: 1,
            version_minor: 2,
            version_build: 3,
            cap_flags: Capability::empty(),
        }
    }
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {}
