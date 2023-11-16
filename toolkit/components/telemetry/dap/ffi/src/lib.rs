/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::error::Error;
use std::io::Cursor;

use prio::vdaf::prio3::Prio3Sum;
use prio::vdaf::prio3::Prio3SumVec;
use thin_vec::ThinVec;

pub mod types;
use types::HpkeConfig;
use types::PlaintextInputShare;
use types::Report;
use types::ReportID;
use types::ReportMetadata;
use types::Time;

use prio::codec::Encode;
use prio::codec::{decode_u16_items, encode_u32_items};
use prio::flp::types::{Sum, SumVec};
use prio::vdaf::prio3::Prio3;
use prio::vdaf::Client;
use prio::vdaf::VdafError;

use crate::types::HpkeCiphertext;

extern "C" {
    pub fn dapHpkeEncryptOneshot(
        aKey: *const u8,
        aKeyLength: u32,
        aInfo: *const u8,
        aInfoLength: u32,
        aAad: *const u8,
        aAadLength: u32,
        aPlaintext: *const u8,
        aPlaintextLength: u32,
        aOutputEncapsulatedKey: &mut ThinVec<u8>,
        aOutputShare: &mut ThinVec<u8>,
    ) -> bool;
}

pub fn new_prio_u8(num_aggregators: u8, bits: u32) -> Result<Prio3Sum, VdafError> {
    if bits > 64 {
        return Err(VdafError::Uncategorized(format!(
            "bit length ({}) exceeds limit for aggregate type (64)",
            bits
        )));
    }

    Prio3::new(num_aggregators, Sum::new(bits as usize)?)
}

pub fn new_prio_vecu8(num_aggregators: u8, len: usize) -> Result<Prio3SumVec, VdafError> {
    let chunk_length = prio::vdaf::prio3::optimal_chunk_length(8 * len);
    Prio3::new(num_aggregators, SumVec::new(8, len, chunk_length)?)
}

pub fn new_prio_vecu16(num_aggregators: u8, len: usize) -> Result<Prio3SumVec, VdafError> {
    let chunk_length = prio::vdaf::prio3::optimal_chunk_length(16 * len);
    Prio3::new(num_aggregators, SumVec::new(16, len, chunk_length)?)
}

enum Role {
    Leader = 2,
    Helper = 3,
}

/// A minimal wrapper around the FFI function which mostly just converts datatypes.
fn hpke_encrypt_wrapper(
    plain_share: &Vec<u8>,
    aad: &Vec<u8>,
    info: &Vec<u8>,
    hpke_config: &HpkeConfig,
) -> Result<HpkeCiphertext, Box<dyn std::error::Error>> {
    let mut encrypted_share = ThinVec::<u8>::new();
    let mut encapsulated_key = ThinVec::<u8>::new();
    unsafe {
        if !dapHpkeEncryptOneshot(
            hpke_config.public_key.as_ptr(),
            hpke_config.public_key.len() as u32,
            info.as_ptr(),
            info.len() as u32,
            aad.as_ptr(),
            aad.len() as u32,
            plain_share.as_ptr(),
            plain_share.len() as u32,
            &mut encapsulated_key,
            &mut encrypted_share,
        ) {
            return Err(Box::from("Encryption failed."));
        }
    }

    Ok(HpkeCiphertext {
        config_id: hpke_config.id,
        enc: encapsulated_key.to_vec(),
        payload: encrypted_share.to_vec(),
    })
}

trait Shardable {
    fn shard(
        &self,
        nonce: &[u8; 16],
    ) -> Result<(Vec<u8>, Vec<Vec<u8>>), Box<dyn std::error::Error>>;
}

impl Shardable for u8 {
    fn shard(
        &self,
        nonce: &[u8; 16],
    ) -> Result<(Vec<u8>, Vec<Vec<u8>>), Box<dyn std::error::Error>> {
        let prio = new_prio_u8(2, 2)?;

        let (public_share, input_shares) = prio.shard(&(*self as u128), nonce)?;

        debug_assert_eq!(input_shares.len(), 2);

        let encoded_input_shares = input_shares.iter().map(|s| s.get_encoded()).collect();
        let encoded_public_share = public_share.get_encoded();
        Ok((encoded_public_share, encoded_input_shares))
    }
}

impl Shardable for ThinVec<u8> {
    fn shard(
        &self,
        nonce: &[u8; 16],
    ) -> Result<(Vec<u8>, Vec<Vec<u8>>), Box<dyn std::error::Error>> {
        let prio = new_prio_vecu8(2, self.len())?;

        let measurement: Vec<u128> = self.iter().map(|e| (*e as u128)).collect();
        let (public_share, input_shares) = prio.shard(&measurement, nonce)?;

        debug_assert_eq!(input_shares.len(), 2);

        let encoded_input_shares = input_shares.iter().map(|s| s.get_encoded()).collect();
        let encoded_public_share = public_share.get_encoded();
        Ok((encoded_public_share, encoded_input_shares))
    }
}

impl Shardable for ThinVec<u16> {
    fn shard(
        &self,
        nonce: &[u8; 16],
    ) -> Result<(Vec<u8>, Vec<Vec<u8>>), Box<dyn std::error::Error>> {
        let prio = new_prio_vecu16(2, self.len())?;

        let measurement: Vec<u128> = self.iter().map(|e| (*e as u128)).collect();
        let (public_share, input_shares) = prio.shard(&measurement, nonce)?;

        debug_assert_eq!(input_shares.len(), 2);

        let encoded_input_shares = input_shares.iter().map(|s| s.get_encoded()).collect();
        let encoded_public_share = public_share.get_encoded();
        Ok((encoded_public_share, encoded_input_shares))
    }
}

/// Pre-fill the info part of the HPKE sealing with the constants from the standard.
fn make_base_info() -> Vec<u8> {
    let mut info = Vec::<u8>::new();
    const START: &[u8] = "dap-07 input share".as_bytes();
    info.extend(START);
    const FIXED: u8 = 1;
    info.push(FIXED);

    info
}

fn select_hpke_config(configs: Vec<HpkeConfig>) -> Result<HpkeConfig, Box<dyn Error>> {
    for config in configs {
        if config.kem_id == 0x20 /* DHKEM(X25519, HKDF-SHA256) */ &&
        config.kdf_id == 0x01 /* HKDF-SHA256 */ &&
        config.aead_id == 0x01
        /* AES-128-GCM */
        {
            return Ok(config);
        }
    }

    Err("No suitable HPKE config found.".into())
}

/// This function creates a full report - ready to send - for a measurement.
///
/// To do that it also needs the HPKE configurations for the endpoints and some
/// additional data which is part of the authentication.
fn get_dap_report_internal<T: Shardable>(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: &T,
    task_id: &[u8; 32],
    time_precision: u64,
) -> Result<Report, Box<dyn std::error::Error>> {
    let leader_hpke_configs: Vec<HpkeConfig> =
        decode_u16_items(&(), &mut Cursor::new(leader_hpke_config_encoded))?;
    let leader_hpke_config = select_hpke_config(leader_hpke_configs)?;
    let helper_hpke_configs: Vec<HpkeConfig> =
        decode_u16_items(&(), &mut Cursor::new(helper_hpke_config_encoded))?;
    let helper_hpke_config = select_hpke_config(helper_hpke_configs)?;

    let report_id = ReportID::generate();
    let (encoded_public_share, encoded_input_shares) = measurement.shard(report_id.as_ref())?;

    let plaintext_input_shares: Vec<Vec<u8>> = encoded_input_shares
        .into_iter()
        .map(|encoded_input_share| {
            PlaintextInputShare {
                extensions: Vec::new(),
                payload: encoded_input_share,
            }
            .get_encoded()
        })
        .collect();

    let metadata = ReportMetadata {
        report_id,
        time: Time::generate(time_precision),
    };

    // This quote from the standard describes which info and aad to use for the encryption:
    //     enc, payload = SealBase(pk,
    //         "dap-02 input share" || 0x01 || server_role,
    //         task_id || metadata || public_share, input_share)
    // https://www.ietf.org/archive/id/draft-ietf-ppm-dap-02.html#name-upload-request
    let mut info = make_base_info();

    let mut aad = Vec::from(*task_id);
    metadata.encode(&mut aad);
    encode_u32_items(&mut aad, &(), &encoded_public_share);

    info.push(Role::Leader as u8);

    let leader_payload =
        hpke_encrypt_wrapper(&plaintext_input_shares[0], &aad, &info, &leader_hpke_config)?;

    *info.last_mut().unwrap() = Role::Helper as u8;

    let helper_payload =
        hpke_encrypt_wrapper(&plaintext_input_shares[1], &aad, &info, &helper_hpke_config)?;

    Ok(Report {
        metadata,
        public_share: encoded_public_share,
        leader_encrypted_input_share: leader_payload,
        helper_encrypted_input_share: helper_payload,
    })
}

/// Wraps the function above with minor C interop.
/// Mostly it turns any error result into a return value of false.
#[no_mangle]
pub extern "C" fn dapGetReportU8(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: u8,
    task_id: &ThinVec<u8>,
    time_precision: u64,
    out_report: &mut ThinVec<u8>,
) -> bool {
    assert_eq!(task_id.len(), 32);

    if let Ok(report) = get_dap_report_internal::<u8>(
        leader_hpke_config_encoded,
        helper_hpke_config_encoded,
        &measurement,
        &task_id.as_slice().try_into().unwrap(),
        time_precision,
    ) {
        let encoded_report = report.get_encoded();
        out_report.extend(encoded_report);

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn dapGetReportVecU8(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: &ThinVec<u8>,
    task_id: &ThinVec<u8>,
    time_precision: u64,
    out_report: &mut ThinVec<u8>,
) -> bool {
    assert_eq!(task_id.len(), 32);

    if let Ok(report) = get_dap_report_internal::<ThinVec<u8>>(
        leader_hpke_config_encoded,
        helper_hpke_config_encoded,
        measurement,
        &task_id.as_slice().try_into().unwrap(),
        time_precision,
    ) {
        let encoded_report = report.get_encoded();
        out_report.extend(encoded_report);

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn dapGetReportVecU16(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: &ThinVec<u16>,
    task_id: &ThinVec<u8>,
    time_precision: u64,
    out_report: &mut ThinVec<u8>,
) -> bool {
    assert_eq!(task_id.len(), 32);

    if let Ok(report) = get_dap_report_internal::<ThinVec<u16>>(
        leader_hpke_config_encoded,
        helper_hpke_config_encoded,
        measurement,
        &task_id.as_slice().try_into().unwrap(),
        time_precision,
    ) {
        let encoded_report = report.get_encoded();
        out_report.extend(encoded_report);

        true
    } else {
        false
    }
}
