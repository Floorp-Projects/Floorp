/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Allow dead code in this module, since it's all packet consts anyways.
#![allow(dead_code)]

pub const HID_RPT_SIZE: usize = 64;
pub const U2FAPDUHEADER_SIZE: usize = 7;
pub const CID_BROADCAST: [u8; 4] = [0xff, 0xff, 0xff, 0xff];
pub const TYPE_MASK: u8 = 0x80;
pub const TYPE_INIT: u8 = 0x80;
pub const TYPE_CONT: u8 = 0x80;

// Size of data chunk expected in U2F Init USB HID Packets
pub const INIT_DATA_SIZE: usize = HID_RPT_SIZE - 7;
// Size of data chunk expected in U2F Cont USB HID Packets
pub const CONT_DATA_SIZE: usize = HID_RPT_SIZE - 5;

pub const PARAMETER_SIZE: usize = 32;

pub const FIDO_USAGE_PAGE: u16 = 0xf1d0; // FIDO alliance HID usage page
pub const FIDO_USAGE_U2FHID: u16 = 0x01; // U2FHID usage for top-level collection
pub const FIDO_USAGE_DATA_IN: u8 = 0x20; // Raw IN data report
pub const FIDO_USAGE_DATA_OUT: u8 = 0x21; // Raw OUT data report

// General pub constants

pub const U2FHID_IF_VERSION: u32 = 2; // Current interface implementation version
pub const U2FHID_FRAME_TIMEOUT: u32 = 500; // Default frame timeout in ms
pub const U2FHID_TRANS_TIMEOUT: u32 = 3000; // Default message timeout in ms

// U2FHID native commands
pub const U2FHID_PING: u8 = (TYPE_INIT | 0x01); // Echo data through local processor only
pub const U2FHID_MSG: u8 = (TYPE_INIT | 0x03); // Send U2F message frame
pub const U2FHID_LOCK: u8 = (TYPE_INIT | 0x04); // Send lock channel command
pub const U2FHID_INIT: u8 = (TYPE_INIT | 0x06); // Channel initialization
pub const U2FHID_WINK: u8 = (TYPE_INIT | 0x08); // Send device identification wink
pub const U2FHID_ERROR: u8 = (TYPE_INIT | 0x3f); // Error response

// U2FHID_MSG commands
pub const U2F_VENDOR_FIRST: u8 = (TYPE_INIT | 0x40); // First vendor defined command
pub const U2F_VENDOR_LAST: u8 = (TYPE_INIT | 0x7f); // Last vendor defined command
pub const U2F_REGISTER: u8 = 0x01; // Registration command
pub const U2F_AUTHENTICATE: u8 = 0x02; // Authenticate/sign command
pub const U2F_VERSION: u8 = 0x03; // Read version string command

// U2F_REGISTER command defines
pub const U2F_REGISTER_ID: u8 = 0x05; // Version 2 registration identifier
pub const U2F_REGISTER_HASH_ID: u8 = 0x00; // Version 2 hash identintifier

// U2F_AUTHENTICATE command defines
pub const U2F_REQUEST_USER_PRESENCE: u8 = 0x03; // Verify user presence and sign
pub const U2F_CHECK_IS_REGISTERED: u8 = 0x07; // Check if the key handle is registered

// U2FHID_INIT command defines
pub const INIT_NONCE_SIZE: usize = 8; // Size of channel initialization challenge
pub const CAPFLAG_WINK: u8 = 0x01; // Device supports WINK command
pub const CAPFLAG_LOCK: u8 = 0x02; // Device supports LOCK command

// Low-level error codes. Return as negatives.

pub const ERR_NONE: u8 = 0x00; // No error
pub const ERR_INVALID_CMD: u8 = 0x01; // Invalid command
pub const ERR_INVALID_PAR: u8 = 0x02; // Invalid parameter
pub const ERR_INVALID_LEN: u8 = 0x03; // Invalid message length
pub const ERR_INVALID_SEQ: u8 = 0x04; // Invalid message sequencing
pub const ERR_MSG_TIMEOUT: u8 = 0x05; // Message has timed out
pub const ERR_CHANNEL_BUSY: u8 = 0x06; // Channel busy
pub const ERR_LOCK_REQUIRED: u8 = 0x0a; // Command requires channel lock
pub const ERR_INVALID_CID: u8 = 0x0b; // Command not allowed on this cid
pub const ERR_OTHER: u8 = 0x7f; // Other unspecified error

// These are ISO 7816-4 defined response status words.
pub const SW_NO_ERROR: [u8; 2] = [0x90, 0x00];
pub const SW_CONDITIONS_NOT_SATISFIED: [u8; 2] = [0x69, 0x85];
pub const SW_WRONG_DATA: [u8; 2] = [0x6A, 0x80];
pub const SW_WRONG_LENGTH: [u8; 2] = [0x67, 0x00];
