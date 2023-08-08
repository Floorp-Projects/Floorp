/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(clippy::large_enum_variant)]
#![allow(clippy::upper_case_acronyms)]
#![allow(clippy::bool_to_int_with_if)]

#[macro_use]
mod util;

#[cfg(target_os = "linux")]
extern crate libudev;

#[cfg(target_os = "freebsd")]
extern crate devd_rs;

#[cfg(target_os = "macos")]
extern crate core_foundation;

extern crate libc;
#[macro_use]
extern crate log;
extern crate rand;
extern crate runloop;

#[macro_use]
extern crate bitflags;

mod consts;
mod manager;
mod statemachine;
mod status_update;
mod transport;
mod u2fprotocol;
mod u2ftypes;

pub mod authenticatorservice;
pub mod crypto;
pub mod ctap2;
pub mod errors;
pub mod statecallback;
pub use ctap2::attestation::AttestationObject;
pub use ctap2::commands::client_pin::{Pin, PinError};
pub use ctap2::commands::get_assertion::{Assertion, GetAssertionResult};
pub use ctap2::commands::get_info::AuthenticatorInfo;
pub use statemachine::StateMachine;
pub use status_update::{InteractiveRequest, StatusPinUv, StatusUpdate};
pub use transport::{FidoDevice, FidoDeviceIO, FidoProtocol, VirtualFidoDevice};

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
