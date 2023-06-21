/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::Deserialize;
use std::ffi::c_void;
use std::fs::File;
use std::io::Cursor;

use thin_vec::ThinVec;

use dap_ffi::types::Report;

use prio::codec::{Decode, Encode};

#[no_mangle]
pub extern "C" fn dap_test_encoding() {
    let r = Report::new_dummy();
    let mut encoded = Vec::<u8>::new();
    Report::encode(&r, &mut encoded);
    let decoded = Report::decode(&mut Cursor::new(&encoded)).expect("Report decoding failed!");
    if r != decoded {
        println!("Report:");
        println!("{:?}", r);
        println!("Encoded Report:");
        println!("{:?}", encoded);
        println!("Decoded Report:");
        println!("{:?}", decoded);
        panic!("Report changed after encoding & decoding.");
    }
}

extern "C" {
    pub fn dapHpkeEncrypt(
        aContext: *mut c_void,
        aAad: *mut u8,
        aAadLength: u32,
        aPlaintext: *mut u8,
        aPlaintextLength: u32,
        aOutputShare: &mut ThinVec<u8>,
    ) -> bool;
    pub fn dapSetupHpkeContextForTesting(
        aKey: *const u8,
        aKeyLength: u32,
        aInfo: *mut u8,
        aInfoLength: u32,
        aPkEm: *const u8,
        aPkEmLength: u32,
        aSkEm: *const u8,
        aSkEmLength: u32,
        aOutputEncapsulatedKey: &mut ThinVec<u8>,
    ) -> *mut c_void;
    pub fn dapDestroyHpkeContext(aContext: *mut c_void);
}

struct HpkeContext(*mut c_void);

impl Drop for HpkeContext {
    fn drop(&mut self) {
        unsafe {
            dapDestroyHpkeContext(self.0);
        }
    }
}

type Testsuites = Vec<CiphersuiteTest>;

#[derive(Debug, Deserialize)]
pub struct HexString(#[serde(with = "hex")] Vec<u8>);
impl AsRef<[u8]> for HexString {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}
#[allow(dead_code)]
#[derive(Debug, Deserialize)]
struct CiphersuiteTest {
    mode: i64,
    kem_id: i64,
    kdf_id: i64,
    aead_id: i64,
    info: HexString,
    #[serde(rename = "ikmR")]
    ikm_r: HexString,
    #[serde(rename = "ikmE")]
    ikm_e: HexString,
    #[serde(rename = "skRm")]
    sk_r_m: HexString,
    #[serde(rename = "skEm")]
    sk_e_m: HexString,
    #[serde(rename = "pkRm")]
    pk_r_m: HexString,
    #[serde(rename = "pkEm")]
    pk_e_m: HexString,
    enc: HexString,
    shared_secret: HexString,
    key_schedule_context: HexString,
    secret: HexString,
    key: HexString,
    base_nonce: HexString,
    exporter_secret: HexString,
    encryptions: Vec<Encryption>,
    exports: Vec<Export>,
    psk: Option<HexString>,
    psk_id: Option<HexString>,
    ikm_s: Option<HexString>,
    sk_sm: Option<HexString>,
    pk_sm: Option<HexString>,
}

#[derive(Debug, Deserialize)]
pub struct Encryption {
    pub aad: HexString,
    pub ciphertext: HexString,
    pub nonce: HexString,
    pub plaintext: HexString,
}

#[derive(Debug, Deserialize)]
pub struct Export {
    pub exporter_context: HexString,
    #[serde(rename = "L")]
    pub length: i64,
    pub exported_value: HexString,
}

#[no_mangle]
pub extern "C" fn dap_test_hpke_encrypt() {
    let file = File::open("hpke-vectors.json").unwrap();
    let tests: Testsuites = serde_json::from_reader(file).unwrap();

    let mut have_tested = false;

    for (test_idx, test) in tests.into_iter().enumerate() {
        // Mode must be "Base"
        if test.mode != 0
         // KEM must be DHKEM(X25519, HKDF-SHA256)
         || test.kem_id != 32
         // KDF must be HKDF-SHA256
         || test.kdf_id != 1
         // AEAD must be AES-128-GCM
         || test.aead_id != 1
        {
            continue;
        }

        have_tested = true;

        let mut pk_r_serialized = test.pk_r_m.0;
        let mut info = test.info.0;
        let mut pk_e_serialized = test.pk_e_m.0;
        let mut sk_e_serialized = test.sk_e_m.0;

        let mut encapsulated_key = ThinVec::<u8>::new();

        let ctx = HpkeContext(unsafe {
            dapSetupHpkeContextForTesting(
                pk_r_serialized.as_mut_ptr(),
                pk_r_serialized.len().try_into().unwrap(),
                info.as_mut_ptr(),
                info.len().try_into().unwrap(),
                pk_e_serialized.as_mut_ptr(),
                pk_e_serialized.len().try_into().unwrap(),
                sk_e_serialized.as_mut_ptr(),
                sk_e_serialized.len().try_into().unwrap(),
                &mut encapsulated_key,
            )
        });
        if ctx.0.is_null() {
            panic!("Failed to set up HPKE context.");
        }
        if encapsulated_key != test.enc.0 {
            panic!("Encapsulated key is wrong!");
        }

        for (encryption_idx, encryption) in test.encryptions.into_iter().enumerate() {
            let mut encrypted_share = ThinVec::<u8>::new();

            let mut aad = encryption.aad.0.clone();
            let mut pt = encryption.plaintext.0.clone();
            unsafe {
                dapHpkeEncrypt(
                    ctx.0,
                    aad.as_mut_ptr(),
                    aad.len().try_into().unwrap(),
                    pt.as_mut_ptr(),
                    pt.len().try_into().unwrap(),
                    &mut encrypted_share,
                );
            }

            if encrypted_share != encryption.ciphertext.0 {
                println!("Test: {}, Encryption: {}", test_idx, encryption_idx);
                println!("Expected:");
                println!("{:?}", encryption.ciphertext.0);
                println!("Actual:");
                println!("{:?}", encrypted_share);
                panic!("Encryption outputs did not match!");
            }
        }
    }

    assert!(have_tested);
}
