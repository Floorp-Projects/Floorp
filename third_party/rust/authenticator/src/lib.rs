/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(clippy::large_enum_variant)]
#![allow(clippy::upper_case_acronyms)]
#![allow(clippy::bool_to_int_with_if)]

#[macro_use]
mod util;

#[cfg(any(target_os = "linux"))]
extern crate libudev;

#[cfg(any(target_os = "freebsd"))]
extern crate devd_rs;

#[cfg(any(target_os = "macos"))]
extern crate core_foundation;

extern crate libc;
#[macro_use]
extern crate log;
extern crate rand;
extern crate runloop;

#[macro_use]
extern crate bitflags;

pub mod authenticatorservice;
mod consts;
mod statemachine;
mod u2fprotocol;
mod u2ftypes;

mod manager;

pub mod ctap2;
pub use ctap2::attestation::AttestationObject;
pub use ctap2::client_data::CollectedClientData;
pub use ctap2::commands::client_pin::{Pin, PinError};
pub use ctap2::commands::get_assertion::Assertion;
pub use ctap2::commands::get_info::AuthenticatorInfo;
pub use ctap2::GetAssertionResult;

pub mod errors;
pub mod statecallback;
mod transport;
mod virtualdevices;

mod status_update;
pub use status_update::*;

mod crypto;
pub use crypto::COSEAlgorithm;

// Keep this in sync with the constants in u2fhid-capi.h.
bitflags! {
    pub struct RegisterFlags: u64 {
        const REQUIRE_RESIDENT_KEY        = 1;
        const REQUIRE_USER_VERIFICATION   = 2;
        const REQUIRE_PLATFORM_ATTACHMENT = 4;
    }
}
bitflags! {
    pub struct SignFlags: u64 {
        const REQUIRE_USER_VERIFICATION = 1;
    }
}
bitflags! {
    pub struct AuthenticatorTransports: u8 {
        const USB = 1;
        const NFC = 2;
        const BLE = 4;
    }
}

#[derive(Debug, Clone)]
pub struct KeyHandle {
    pub credential: Vec<u8>,
    pub transports: AuthenticatorTransports,
}

pub type AppId = Vec<u8>;

#[derive(Debug)]
pub enum RegisterResult {
    CTAP1(Vec<u8>, u2ftypes::U2FDeviceInfo),
    CTAP2(AttestationObject),
}

#[derive(Debug)]
pub enum SignResult {
    CTAP1(AppId, Vec<u8>, Vec<u8>, u2ftypes::U2FDeviceInfo),
    CTAP2(GetAssertionResult),
}

pub type ResetResult = ();

pub type Result<T> = std::result::Result<T, errors::AuthenticatorError>;

#[cfg(test)]
#[macro_use]
extern crate assert_matches;

#[cfg(fuzzing)]
pub use consts::*;
#[cfg(fuzzing)]
pub use u2fprotocol::*;
#[cfg(fuzzing)]
pub use u2ftypes::*;
