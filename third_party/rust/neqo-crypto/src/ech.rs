// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    err::{ssl::SSL_ERROR_ECH_RETRY_WITH_ECH, Error, Res},
    experimental_api,
    p11::{
        self, Item, PrivateKey, PublicKey, SECITEM_FreeItem, SECItem, SECKEYPrivateKey,
        SECKEYPublicKey, Slot,
    },
    ssl::{PRBool, PRFileDesc},
};
use neqo_common::qtrace;
use std::{
    convert::TryFrom,
    ffi::CString,
    os::raw::{c_char, c_uint},
    ptr::{addr_of_mut, null_mut},
};

pub use crate::{
    p11::{HpkeAeadId as AeadId, HpkeKdfId as KdfId, HpkeKemId as KemId},
    ssl::HpkeSymmetricSuite as SymmetricSuite,
};

experimental_api!(SSL_EnableTls13GreaseEch(
    fd: *mut PRFileDesc,
    enabled: PRBool,
));

experimental_api!(SSL_GetEchRetryConfigs(
    fd: *mut PRFileDesc,
    config: *mut SECItem,
));

experimental_api!(SSL_SetClientEchConfigs(
    fd: *mut PRFileDesc,
    config_list: *const u8,
    config_list_len: c_uint,
));

experimental_api!(SSL_SetServerEchConfigs(
    fd: *mut PRFileDesc,
    pk: *const SECKEYPublicKey,
    sk: *const SECKEYPrivateKey,
    record: *const u8,
    record_len: c_uint,
));

experimental_api!(SSL_EncodeEchConfigId(
    config_id: u8,
    public_name: *const c_char,
    max_name_len: c_uint,
    kem_id: KemId::Type,
    pk: *const SECKEYPublicKey,
    hpke_suites: *const SymmetricSuite,
    hpke_suite_count: c_uint,
    out: *mut u8,
    out_len: *mut c_uint,
    max_len: c_uint,
));

/// Convert any result that contains an ECH error into a result with an `EchRetry`.
pub fn convert_ech_error(fd: *mut PRFileDesc, err: Error) -> Error {
    if let Error::NssError {
        code: SSL_ERROR_ECH_RETRY_WITH_ECH,
        ..
    } = &err
    {
        let mut item = Item::make_empty();
        if unsafe { SSL_GetEchRetryConfigs(fd, &mut item).is_err() } {
            return Error::InternalError;
        }
        let buf = unsafe {
            let slc = std::slice::from_raw_parts(item.data, usize::try_from(item.len).unwrap());
            let buf = Vec::from(slc);
            SECITEM_FreeItem(&mut item, PRBool::from(false));
            buf
        };
        Error::EchRetry(buf)
    } else {
        err
    }
}

/// Generate a key pair for encrypted client hello (ECH).
///
/// # Errors
/// When NSS fails to generate a key pair or when the KEM is not supported.
/// # Panics
/// When underlying types aren't large enough to hold keys.  So never.
pub fn generate_keys() -> Res<(PrivateKey, PublicKey)> {
    let slot = Slot::internal()?;

    let oid_data = unsafe { p11::SECOID_FindOIDByTag(p11::SECOidTag::SEC_OID_CURVE25519) };
    let oid = unsafe { oid_data.as_ref() }.ok_or(Error::InternalError)?;
    let oid_slc =
        unsafe { std::slice::from_raw_parts(oid.oid.data, usize::try_from(oid.oid.len).unwrap()) };
    let mut params: Vec<u8> = Vec::with_capacity(oid_slc.len() + 2);
    params.push(u8::try_from(p11::SEC_ASN1_OBJECT_ID).unwrap());
    params.push(u8::try_from(oid.oid.len).unwrap());
    params.extend_from_slice(oid_slc);

    let mut public_ptr: *mut SECKEYPublicKey = null_mut();
    let mut param_item = Item::wrap(&params);

    // If we have tracing on, try to ensure that key data can be read.
    let insensitive_secret_ptr = if log::log_enabled!(log::Level::Trace) {
        unsafe {
            p11::PK11_GenerateKeyPairWithOpFlags(
                *slot,
                p11::CK_MECHANISM_TYPE::from(p11::CKM_EC_KEY_PAIR_GEN),
                addr_of_mut!(param_item).cast(),
                &mut public_ptr,
                p11::PK11_ATTR_SESSION | p11::PK11_ATTR_INSENSITIVE | p11::PK11_ATTR_PUBLIC,
                p11::CK_FLAGS::from(p11::CKF_DERIVE),
                p11::CK_FLAGS::from(p11::CKF_DERIVE),
                null_mut(),
            )
        }
    } else {
        null_mut()
    };
    assert_eq!(insensitive_secret_ptr.is_null(), public_ptr.is_null());
    let secret_ptr = if insensitive_secret_ptr.is_null() {
        unsafe {
            p11::PK11_GenerateKeyPairWithOpFlags(
                *slot,
                p11::CK_MECHANISM_TYPE::from(p11::CKM_EC_KEY_PAIR_GEN),
                addr_of_mut!(param_item).cast(),
                &mut public_ptr,
                p11::PK11_ATTR_SESSION | p11::PK11_ATTR_SENSITIVE | p11::PK11_ATTR_PRIVATE,
                p11::CK_FLAGS::from(p11::CKF_DERIVE),
                p11::CK_FLAGS::from(p11::CKF_DERIVE),
                null_mut(),
            )
        }
    } else {
        insensitive_secret_ptr
    };
    assert_eq!(secret_ptr.is_null(), public_ptr.is_null());
    let sk = PrivateKey::from_ptr(secret_ptr)?;
    let pk = PublicKey::from_ptr(public_ptr)?;
    qtrace!("Generated key pair: sk={:?} pk={:?}", sk, pk);
    Ok((sk, pk))
}

/// Encode a configuration for encrypted client hello (ECH).
///
/// # Errors
/// When NSS fails to generate a valid configuration encoding (i.e., unlikely).
pub fn encode_config(config: u8, public_name: &str, pk: &PublicKey) -> Res<Vec<u8>> {
    // A sensible fixed value for the maximum length of a name.
    const MAX_NAME_LEN: c_uint = 64;
    // Enable a selection of suites.
    // NSS supports SHA-512 as well, which could be added here.
    const SUITES: &[SymmetricSuite] = &[
        SymmetricSuite {
            kdfId: KdfId::HpkeKdfHkdfSha256,
            aeadId: AeadId::HpkeAeadAes128Gcm,
        },
        SymmetricSuite {
            kdfId: KdfId::HpkeKdfHkdfSha256,
            aeadId: AeadId::HpkeAeadChaCha20Poly1305,
        },
        SymmetricSuite {
            kdfId: KdfId::HpkeKdfHkdfSha384,
            aeadId: AeadId::HpkeAeadAes128Gcm,
        },
        SymmetricSuite {
            kdfId: KdfId::HpkeKdfHkdfSha384,
            aeadId: AeadId::HpkeAeadChaCha20Poly1305,
        },
    ];

    let name = CString::new(public_name)?;
    let mut encoded = [0; 1024];
    let mut encoded_len = 0;
    unsafe {
        SSL_EncodeEchConfigId(
            config,
            name.as_ptr(),
            MAX_NAME_LEN,
            KemId::HpkeDhKemX25519Sha256,
            **pk,
            SUITES.as_ptr(),
            c_uint::try_from(SUITES.len())?,
            encoded.as_mut_ptr(),
            &mut encoded_len,
            c_uint::try_from(encoded.len())?,
        )?;
    }
    Ok(Vec::from(&encoded[..usize::try_from(encoded_len)?]))
}
