/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This file contains structs for use in the DAP protocol and implements TLS compatible
//! serialization/deserialization as required for the wire protocol.
//!
//! The current draft standard with the definition of these structs is available here:
//! https://github.com/ietf-wg-ppm/draft-ietf-ppm-dap
//! This code is based on version 07 of the standard available here:
//! https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html

use prio::codec::{
    decode_u16_items, decode_u32_items, encode_u16_items, encode_u32_items, CodecError, Decode,
    Encode,
};
use std::io::{Cursor, Read};
use std::time::{SystemTime, UNIX_EPOCH};

use rand::{Rng, RngCore};

/// opaque TaskId[32];
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-task-configuration
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

/// uint64 Time;
/// seconds elapsed since start of UNIX epoch
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-protocol-definition
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

impl Time {
    /// Generates a Time for the current system time rounded to the desired precision.
    pub fn generate(time_precision: u64) -> Time {
        let now_secs = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("Failed to get time.")
            .as_secs();
        let timestamp = (now_secs / time_precision) * time_precision;
        Time(timestamp)
    }
}

/// struct {
///     ExtensionType extension_type;
///     opaque extension_data<0..2^16-1>;
/// } Extension;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-upload-extensions
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
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-upload-extensions
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

/// struct {
///     Extension extensions<0..2^16-1>;
///     opaque payload<0..2^32-1>;
/// } PlaintextInputShare;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#section-4.4.2-9
#[derive(Debug)]
pub struct PlaintextInputShare {
    pub extensions: Vec<Extension>,
    pub payload: Vec<u8>,
}

impl Encode for PlaintextInputShare {
    fn encode(&self, bytes: &mut Vec<u8>) {
        encode_u16_items(bytes, &(), &self.extensions);
        encode_u32_items(bytes, &(), &self.payload);
    }
}

/// Identifier for a server's HPKE configuration
/// uint8 HpkeConfigId;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-protocol-definition
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

/// struct {
///     HpkeConfigId id;
///     HpkeKemId kem_id;
///     HpkeKdfId kdf_id;
///     HpkeAeadId aead_id;
///     HpkePublicKey public_key;
/// } HpkeConfig;
/// opaque HpkePublicKey<1..2^16-1>;
/// uint16 HpkeAeadId; /* Defined in [HPKE] */
/// uint16 HpkeKemId;  /* Defined in [HPKE] */
/// uint16 HpkeKdfId;  /* Defined in [HPKE] */
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-hpke-configuration-request
#[derive(Debug, Clone)]
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
///     HpkeConfigId config_id;    /* config ID */
///     opaque enc<1..2^16-1>;     /* encapsulated HPKE key */
///     opaque payload<1..2^32-1>; /* ciphertext */
/// } HpkeCiphertext;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-protocol-definition
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
        let payload: Vec<u8> = decode_u32_items(&(), bytes)?;

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
        encode_u32_items(bytes, &(), &self.payload);
    }
}

/// opaque ReportID[16];
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-protocol-definition
#[derive(Debug, PartialEq, Eq)]
pub struct ReportID(pub [u8; 16]);

impl Decode for ReportID {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let mut data: [u8; 16] = [0; 16];
        bytes.read_exact(&mut data)?;
        Ok(ReportID(data))
    }
}

impl Encode for ReportID {
    fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.extend_from_slice(&self.0);
    }
}

impl ReportID {
    pub fn generate() -> ReportID {
        ReportID(rand::thread_rng().gen())
    }
}

impl AsRef<[u8; 16]> for ReportID {
    fn as_ref(&self) -> &[u8; 16] {
        &self.0
    }
}

/// struct {
///     ReportID report_id;
///     Time time;
/// } ReportMetadata;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-upload-request
#[derive(Debug, PartialEq)]
pub struct ReportMetadata {
    pub report_id: ReportID,
    pub time: Time,
}

impl Decode for ReportMetadata {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let report_id = ReportID::decode(bytes)?;
        let time = Time::decode(bytes)?;

        Ok(ReportMetadata { report_id, time })
    }
}

impl Encode for ReportMetadata {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.report_id.encode(bytes);
        self.time.encode(bytes);
    }
}

/// struct {
///     ReportMetadata metadata;
///     opaque public_share<0..2^32-1>;
///     HpkeCiphertext leader_encrypted_input_share;
///     HpkeCiphertext helper_encrypted_input_share;
/// } Report;
/// https://www.ietf.org/archive/id/draft-ietf-ppm-dap-07.html#name-upload-request
#[derive(Debug, PartialEq)]
pub struct Report {
    pub metadata: ReportMetadata,
    pub public_share: Vec<u8>,
    pub leader_encrypted_input_share: HpkeCiphertext,
    pub helper_encrypted_input_share: HpkeCiphertext,
}


impl Report {
    /// Creates a minimal report for use in tests.
    pub fn new_dummy() -> Self {
        let mut enc = [0u8; 32];
        rand::thread_rng().fill_bytes(&mut enc);
        let mut payload = [0u8; 200];
        rand::thread_rng().fill_bytes(&mut payload);
        Report {
            metadata: ReportMetadata {
                report_id: ReportID::generate(),
                time: Time::generate(1),
            },
            public_share: vec![],
            leader_encrypted_input_share: HpkeCiphertext { config_id: HpkeConfigId(5), enc: vec![1, 2, 3, 4, 5], payload: vec![6, 7, 8, 9, 10] },
            helper_encrypted_input_share: HpkeCiphertext { config_id: HpkeConfigId(100), enc: enc.into(), payload: payload.into() },
        }
    }
}

impl Decode for Report {
    fn decode(bytes: &mut Cursor<&[u8]>) -> Result<Self, CodecError> {
        let metadata = ReportMetadata::decode(bytes)?;
        let public_share: Vec<u8> = decode_u32_items(&(), bytes)?;
        let leader_encrypted_input_share: HpkeCiphertext = HpkeCiphertext::decode(bytes)?;
        let helper_encrypted_input_share: HpkeCiphertext = HpkeCiphertext::decode(bytes)?;

        let remaining_bytes = bytes.get_ref().len() - (bytes.position() as usize);
        if remaining_bytes == 0 {
            Ok(Report {
                metadata,
                public_share,
                leader_encrypted_input_share,
                helper_encrypted_input_share,
            })
        } else {
            Err(CodecError::BytesLeftOver(remaining_bytes))
        }
    }
}

impl Encode for Report {
    fn encode(&self, bytes: &mut Vec<u8>) {
        self.metadata.encode(bytes);
        encode_u32_items(bytes, &(), &self.public_share);
        self.leader_encrypted_input_share.encode(bytes);
        self.helper_encrypted_input_share.encode(bytes);
    }
}
