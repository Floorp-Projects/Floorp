/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This file contains structs for use in the DAP protocol and implements TLS compatible
//! serialization/deserialization as required for the wire protocol.
//!
//! The current draft standard with the definition of these structs is available here:
//! https://github.com/ietf-wg-ppm/draft-ietf-ppm-dap

use prio::codec::{decode_u16_items, encode_u16_items, CodecError, Decode, Encode};
use std::io::{Cursor, Read};
use std::time::{SystemTime, UNIX_EPOCH};

use rand::Rng;

/// opaque TaskId[32];
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-task-configuration
#[derive(Debug, PartialEq, Eq)]
pub struct TaskID(pub [u8; 32]);

impl Decode for TaskID {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        // this should probably be available in codec...?
        let mut data: [u8; 32] = [0; 32];
        bytes.read_exact(&mut data)?;
        Ok(TaskID(data))
    }
}

impl Encode for TaskID {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&self.0);
    }
}

/// Time uint64;
/// seconds elapsed since start of UNIX epoch
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-protocol-definition
#[derive(Debug, PartialEq, Eq)]
pub struct Time(pub u64);

impl Decode for Time {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(Time(u64::decode(bytes)?))
    }
}

impl Encode for Time {
    fn encode(&self, bytes: &mut Vec<u8>) {
        u64::encode(&self.0, bytes);
    }
}

/// struct {
///     ExtensionType extension_type;
///     opaque extension_data<0..2^16-1>;
/// } Extension;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-upload-extensions
#[derive(Debug, PartialEq)]
pub struct Extension {
    extension_type: ExtensionType,
    extension_data: Vec<u8>,
}

impl Decode for Extension {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let extension_type = ExtensionType::from_u16(u16::decode(bytes)?);
        let extension_data: Vec<u8> = decode_u16_items(&(), bytes)?;

        Ok(Extension {
            extension_type,
            extension_data,
        })
    }
}

impl Encode for Extension {
    fn encode(&self, bytes: &mut Vec<u8>) {
        (self.extension_type as u16).encode(bytes);
        encode_u16_items(bytes, &(), &self.extension_data);
    }
}

/// enum {
///     TBD(0),
///     (65535)
/// } ExtensionType;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-upload-extensions
#[derive(Debug, PartialEq, Clone, Copy)]
#[repr(u16)]
enum ExtensionType {
    Tbd = 0,
}

impl ExtensionType {
    fn from_u16(value: u16) -> ExtensionType {
        match value {
            0 => ExtensionType::Tbd,
            _ => panic!("Unknown value for Extension Type: {}", value),
        }
    }
}

/// Identifier for a server's HPKE configuration
/// uint8 HpkeConfigId;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-protocol-definition
#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct HpkeConfigId(u8);

impl Decode for HpkeConfigId {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(HpkeConfigId(u8::decode(bytes)?))
    }
}

impl Encode for HpkeConfigId {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.0.encode(bytes);
    }
}

#[derive(Debug)]
pub struct HpkeConfig {
    pub id: HpkeConfigId,
    pub kem_id: u16,
    pub kdf_id: u16,
    pub aead_id: u16,
    pub public_key: Vec<u8>,
}

impl Decode for HpkeConfig {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        Ok(HpkeConfig {
            id: HpkeConfigId::decode(bytes)?,
            kem_id: u16::decode(bytes)?,
            kdf_id: u16::decode(bytes)?,
            aead_id: u16::decode(bytes)?,
            public_key: decode_u16_items(&(), bytes)?,
        })
    }
}

impl Encode for HpkeConfig {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.id.encode(bytes);
        self.kem_id.encode(bytes);
        self.kdf_id.encode(bytes);
        self.aead_id.encode(bytes);
        encode_u16_items(bytes, &(), &self.public_key);
    }
}

/// An HPKE ciphertext.
/// struct {
///   HpkeConfigId config_id;    // config ID
///   opaque enc<1..2^16-1>;     // encapsulated HPKE key
///   opaque payload<1..2^16-1>; // ciphertext
/// } HpkeCiphertext;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-protocol-definition
#[derive(Debug, PartialEq, Eq)]
pub struct HpkeCiphertext {
    pub config_id: HpkeConfigId,
    pub enc: Vec<u8>,
    pub payload: Vec<u8>,
}

impl Decode for HpkeCiphertext {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let config_id = HpkeConfigId::decode(bytes)?;
        let enc: Vec<u8> = decode_u16_items(&(), bytes)?;
        let payload: Vec<u8> = decode_u16_items(&(), bytes)?;

        Ok(HpkeCiphertext {
            config_id,
            enc,
            payload,
        })
    }
}

impl Encode for HpkeCiphertext {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.config_id.encode(bytes);
        encode_u16_items(bytes, &(), &self.enc);
        encode_u16_items(bytes, &(), &self.payload);
    }
}

/// A nonce used to uniquely identify a report in the context of a DAP task. It
/// includes the timestamp of the current batch and a random 16-byte value.
/// struct {
///   Time time;
///   uint8 rand[16];
/// } Nonce;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-protocol-definition
#[derive(PartialEq, Eq, Debug)]
pub struct Nonce {
    pub time: Time,
    pub rand: [u8; 16],
}

impl Decode for Nonce {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let time = Time::decode(bytes)?;
        let mut data = [0; 16];
        bytes.read_exact(&mut data)?;

        Ok(Nonce { time, rand: data })
    }
}

impl Encode for Nonce {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.time.encode(bytes);
        bytes.extend_from_slice(&self.rand);
    }
}

impl Nonce {
    pub fn generate(time_precision: u64) -> Nonce {
        let now_secs = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("Failed to get time.")
            .as_secs();
        let timestamp = (now_secs / time_precision) * time_precision;
        Nonce {
            time: Time(timestamp),
            rand: rand::thread_rng().gen(),
        }
    }
}

/// struct {
///   TaskID task_id;
///   Nonce nonce;
///   Extension extensions<0..2^16-1>;
///   HpkeCiphertext encrypted_input_shares<1..2^16-1>;
/// } Report;
/// https://ietf-wg-ppm.github.io/draft-ietf-ppm-dap/draft-ietf-ppm-dap.html#name-upload-request
#[derive(Debug, PartialEq)]
pub struct Report {
    pub task_id: TaskID,
    pub nonce: Nonce,
    pub extensions: Vec<Extension>,
    pub encrypted_input_shares: Vec<HpkeCiphertext>,
}

impl Report {
    pub fn new_dummy() -> Self {
        Report {
            task_id: TaskID([0x12; 32]),
            nonce: Nonce::generate(1),
            extensions: vec![],
            encrypted_input_shares: vec![],
        }
    }
}

impl Decode for Report {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let task_id = TaskID::decode(bytes)?;
        let nonce = Nonce::decode(bytes)?;
        let extensions = decode_u16_items(&(), bytes)?;
        let encrypted_input_shares: Vec<HpkeCiphertext> = decode_u16_items(&(), bytes)?;

        let remaining_bytes = bytes.get_ref().len() - (bytes.position() as usize);
        if remaining_bytes == 0 {
            Ok(Report {
                task_id,
                nonce,
                extensions,
                encrypted_input_shares,
            })
        } else {
            Err(CodecError::BytesLeftOver(remaining_bytes))
        }
    }
}

impl Encode for Report {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.task_id.encode(bytes);
        self.nonce.encode(bytes);
        encode_u16_items(bytes, &(), &self.extensions);
        encode_u16_items(bytes, &(), &self.encrypted_input_shares);
    }
}
