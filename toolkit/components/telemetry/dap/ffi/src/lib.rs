/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::io::Cursor;

use thin_vec::ThinVec;

pub mod types;
use types::Extension;
use types::HpkeConfig;
use types::Nonce;
use types::Report;
use types::TaskID;

pub mod prg;
use prg::PrgAes128Alt;

use prio::codec::Encode;
use prio::codec::{encode_u16_items, Decode};
use prio::field::Field128;
use prio::flp::types::Sum;
use prio::vdaf::prio3::Prio3;
use prio::vdaf::Client;
use prio::vdaf::VdafError;

use crate::types::HpkeCiphertext;

type Prio3Aes128SumAlt = Prio3<Sum<Field128>, PrgAes128Alt, 16>;

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

pub fn new_prio(num_aggregators: u8, bits: u32) -> Result<Prio3Aes128SumAlt, VdafError> {
    if bits > 64 {
        return Err(VdafError::Uncategorized(format!(
            "bit length ({}) exceeds limit for aggregate type (64)",
            bits
        )));
    }

    Prio3::new(num_aggregators, Sum::new(bits as usize)?)
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

/// Pre-fill the info part of the HPKE sealing with the constants from the standard.
fn make_base_info() -> Vec<u8> {
    let mut info = Vec::<u8>::new();
    const START: &[u8] = "dap-01 input share".as_bytes();
    info.extend(START);
    const FIXED: u8 = 1;
    info.push(FIXED);

    info
}

/// This function creates a full report - ready to send - for a measurement.
///
/// To do that it also needs the HPKE configurations for the endpoints and some
/// additional data which is part of the authentication.
fn get_dap_report_internal(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: u32,
    task_id: &[u8; 32],
    time_precision: u64,
) -> Result<Report, Box<dyn std::error::Error>> {
    let leader_hpke_config = HpkeConfig::decode(&mut Cursor::new(leader_hpke_config_encoded))?;
    let helper_hpke_config = HpkeConfig::decode(&mut Cursor::new(helper_hpke_config_encoded))?;

    let prio = new_prio(2, 2)?;
    let shards = prio.shard(&(measurement as u128))?;
    debug_assert_eq!(shards.len(), 2);

    let nonce = Nonce::generate(time_precision);

    let mut aad = Vec::from(*task_id);
    nonce.encode(&mut aad);
    let extensions: Vec<Extension> = vec![];
    encode_u16_items(&mut aad, &(), &extensions);

    let mut info = make_base_info();
    info.push(Role::Leader as u8);

    let leader_payload =
        hpke_encrypt_wrapper(&shards[0].get_encoded(), &aad, &info, &leader_hpke_config)?;

    *info.last_mut().unwrap() = Role::Helper as u8;

    let helper_payload =
        hpke_encrypt_wrapper(&shards[1].get_encoded(), &aad, &info, &helper_hpke_config)?;

    Ok(Report {
        task_id: TaskID(*task_id),
        nonce,
        extensions,
        encrypted_input_shares: vec![leader_payload, helper_payload],
    })
}

/// Wraps the function above with minor C interop.
/// Mostly it turns any error result into a return value of false.
#[no_mangle]
pub extern "C" fn dapGetReport(
    leader_hpke_config_encoded: &ThinVec<u8>,
    helper_hpke_config_encoded: &ThinVec<u8>,
    measurement: u32,
    task_id: &ThinVec<u8>,
    time_precision: u64,
    out_report: &mut ThinVec<u8>,
) -> bool {
    assert_eq!(task_id.len(), 32);

    if let Ok(report) = get_dap_report_internal(
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
