#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

use neqo_crypto::constants::{
    Cipher, TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384, TLS_VERSION_1_3,
};
use neqo_crypto::hkdf;
use neqo_crypto::hp::HpKey;
use test_fixture::fixture_init;

fn make_hp(cipher: Cipher) -> HpKey {
    let ikm = hkdf::import_key(TLS_VERSION_1_3, &[0; 16]).expect("import IKM");
    let prk = hkdf::extract(TLS_VERSION_1_3, cipher, None, &ikm).expect("extract works");
    HpKey::extract(TLS_VERSION_1_3, cipher, &prk, "hp").expect("extract label works")
}

#[test]
fn aes128() {
    const EXPECTED: &[u8] = &[
        0x04, 0x7b, 0xda, 0x65, 0xc3, 0x41, 0xcf, 0xbc, 0x5d, 0xe1, 0x75, 0x2b, 0x9d, 0x7d, 0xc3,
        0x14,
    ];

    fixture_init();
    let mask = make_hp(TLS_AES_128_GCM_SHA256)
        .mask(&[0; 16])
        .expect("should produce a mask");
    assert_eq!(mask, EXPECTED);
}

#[test]
fn aes256() {
    const EXPECTED: &[u8] = &[
        0xb5, 0xea, 0xa2, 0x1c, 0x25, 0x77, 0x48, 0x18, 0xbf, 0x25, 0xea, 0xfa, 0xbd, 0x8d, 0x80,
        0x2b,
    ];

    fixture_init();
    let mask = make_hp(TLS_AES_256_GCM_SHA384)
        .mask(&[0; 16])
        .expect("should produce a mask");
    assert_eq!(mask, EXPECTED);
}

#[cfg(feature = "chacha")]
#[test]
fn chacha20_ctr() {
    const EXPECTED: &[u8] = &[
        0x34, 0x11, 0xb3, 0x53, 0x02, 0x0b, 0x16, 0xda, 0x0a, 0x85, 0x5a, 0x52, 0x0d, 0x06, 0x07,
        0x1f, 0x4a, 0xb1, 0xaf, 0xf7, 0x83, 0xa8, 0xf0, 0x29, 0xc3, 0x19, 0xef, 0x57, 0x48, 0xe7,
        0x8e, 0x3e, 0x11, 0x91, 0xe1, 0xd5, 0x92, 0x8f, 0x61, 0x6d, 0x3f, 0x3d, 0x76, 0xb5, 0x29,
        0xf1, 0x62, 0x2f, 0x1e, 0xad, 0xdd, 0x23, 0x59, 0x45, 0xac, 0xd2, 0x19, 0x8a, 0xb4, 0x1f,
        0x2f, 0x52, 0x46, 0x89,
    ];

    fixture_init();
    let mask = make_hp(TLS_CHACHA20_POLY1305_SHA256)
        .mask(&[0; 16])
        .expect("should produce a mask");
    assert_eq!(mask, EXPECTED);
}
