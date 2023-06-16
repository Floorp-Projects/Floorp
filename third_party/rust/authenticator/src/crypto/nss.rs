use super::{CryptoError, DER_OID_P256_BYTES};
use nss_gk_api::p11::{
    PK11Origin, PK11_CreateContextBySymKey, PK11_Decrypt, PK11_DigestFinal, PK11_DigestOp,
    PK11_Encrypt, PK11_GenerateKeyPairWithOpFlags, PK11_GenerateRandom, PK11_HashBuf,
    PK11_ImportSymKey, PK11_PubDeriveWithKDF, PrivateKey, PublicKey,
    SECKEY_DecodeDERSubjectPublicKeyInfo, SECKEY_ExtractPublicKey, SECOidTag, Slot,
    SubjectPublicKeyInfo, AES_BLOCK_SIZE, PK11_ATTR_SESSION, SHA256_LENGTH,
};
use nss_gk_api::{IntoResult, SECItem, SECItemBorrowed, PR_FALSE};
use pkcs11_bindings::{
    CKA_DERIVE, CKA_ENCRYPT, CKA_SIGN, CKD_NULL, CKF_DERIVE, CKM_AES_CBC, CKM_ECDH1_DERIVE,
    CKM_EC_KEY_PAIR_GEN, CKM_SHA256_HMAC, CKM_SHA512_HMAC,
};
use std::convert::TryFrom;
use std::os::raw::{c_int, c_uint};
use std::ptr;

#[cfg(test)]
use super::DER_OID_EC_PUBLIC_KEY_BYTES;

#[cfg(test)]
use nss_gk_api::p11::PK11_ImportDERPrivateKeyInfoAndReturnKey;

impl From<nss_gk_api::Error> for CryptoError {
    fn from(e: nss_gk_api::Error) -> Self {
        CryptoError::Backend(format!("{e}"))
    }
}

pub type Result<T> = std::result::Result<T, CryptoError>;

fn nss_public_key_from_der_spki(spki: &[u8]) -> Result<PublicKey> {
    // TODO: replace this with an nss-gk-api function
    // https://github.com/mozilla/nss-gk-api/issues/7
    let mut spki_item = SECItemBorrowed::wrap(spki);
    let spki_item_ptr: *mut SECItem = spki_item.as_mut();
    let nss_spki = unsafe {
        SubjectPublicKeyInfo::from_ptr(SECKEY_DecodeDERSubjectPublicKeyInfo(spki_item_ptr))?
    };
    let public_key = unsafe { PublicKey::from_ptr(SECKEY_ExtractPublicKey(*nss_spki))? };
    Ok(public_key)
}

/// ECDH using NSS types. Computes the x coordinate of scalar multiplication of `peer_public` by
/// `client_private`.
fn ecdh_nss_raw(client_private: PrivateKey, peer_public: PublicKey) -> Result<Vec<u8>> {
    let ecdh_x_coord = unsafe {
        PK11_PubDeriveWithKDF(
            *client_private,
            *peer_public,
            PR_FALSE,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            CKM_ECDH1_DERIVE,
            CKM_SHA512_HMAC, // unused
            CKA_DERIVE,      // unused
            0,
            CKD_NULL,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
        )
        .into_result()?
    };
    let ecdh_x_coord_bytes = ecdh_x_coord.as_bytes()?;
    Ok(ecdh_x_coord_bytes.to_vec())
}

/// Ephemeral ECDH over P256. Takes a DER SubjectPublicKeyInfo that encodes a public key. Generates
/// an ephemeral P256 key pair. Returns
///  1) the x coordinate of the shared point, and
///  2) the uncompressed SEC 1 encoding of the ephemeral public key.
pub fn ecdhe_p256_raw(peer_spki: &[u8]) -> Result<(Vec<u8>, Vec<u8>)> {
    nss_gk_api::init();

    let peer_public = nss_public_key_from_der_spki(peer_spki)?;

    // Hard-coding the P256 OID here is easier than extracting a group name from peer_public and
    // comparing it with P256. We'll fail in `PK11_GenerateKeyPairWithOpFlags` if peer_public is on
    // the wrong curve.
    let mut oid = SECItemBorrowed::wrap(DER_OID_P256_BYTES);
    let oid_ptr: *mut SECItem = oid.as_mut();

    let slot = Slot::internal()?;

    let mut client_public_ptr = ptr::null_mut();

    // We have to be careful with error handling between the `PK11_GenerateKeyPairWithOpFlags` and
    // `PublicKey::from_ptr` calls here, so I've wrapped them in the same unsafe block as a
    // warning. TODO(jms) Replace this once there is a safer alternative.
    // https://github.com/mozilla/nss-gk-api/issues/1
    let (client_private, client_public) = unsafe {
        let client_private =
            // Type of `param` argument depends on mechanism. For EC keygen it is
            // `SECKEYECParams *` which is a typedef for `SECItem *`.
            PK11_GenerateKeyPairWithOpFlags(
                *slot,
                CKM_EC_KEY_PAIR_GEN,
                oid_ptr.cast(),
                &mut client_public_ptr,
                PK11_ATTR_SESSION,
                CKF_DERIVE,
                CKF_DERIVE,
                ptr::null_mut(),
            )
            .into_result()?;

        let client_public = PublicKey::from_ptr(client_public_ptr)?;

        (client_private, client_public)
    };

    let shared_point = ecdh_nss_raw(client_private, peer_public)?;

    Ok((shared_point, client_public.key_data()?))
}

/// AES-256-CBC encryption for data that is a multiple of the AES block size (16 bytes) in length.
/// Uses the zero IV if `iv` is None.
pub fn encrypt_aes_256_cbc_no_pad(key: &[u8], iv: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    if key.len() != 32 {
        return Err(CryptoError::LibraryFailure);
    }

    let iv = iv.unwrap_or(&[0u8; AES_BLOCK_SIZE]);

    if iv.len() != AES_BLOCK_SIZE {
        return Err(CryptoError::LibraryFailure);
    }

    let in_len = match c_uint::try_from(data.len()) {
        Ok(in_len) => in_len,
        _ => return Err(CryptoError::LibraryFailure),
    };

    if data.len() % AES_BLOCK_SIZE != 0 {
        return Err(CryptoError::LibraryFailure);
    }

    let slot = Slot::internal()?;

    let sym_key = unsafe {
        PK11_ImportSymKey(
            *slot,
            CKM_AES_CBC,
            PK11Origin::PK11_OriginUnwrap,
            CKA_ENCRYPT,
            SECItemBorrowed::wrap(key).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };

    let mut params = SECItemBorrowed::wrap(iv);
    let params_ptr: *mut SECItem = params.as_mut();
    let mut out_len: c_uint = 0;
    let mut out = vec![0; data.len()];
    unsafe {
        PK11_Encrypt(
            *sym_key,
            CKM_AES_CBC,
            params_ptr,
            out.as_mut_ptr(),
            &mut out_len,
            in_len,
            data.as_ptr(),
            in_len,
        )
        .into_result()?
    }
    // CKM_AES_CBC should have output length equal to input length.
    debug_assert_eq!(out_len, in_len);

    Ok(out)
}

/// AES-256-CBC decryption for data that is a multiple of the AES block size (16 bytes) in length.
/// Uses the zero IV if `iv` is None.
pub fn decrypt_aes_256_cbc_no_pad(key: &[u8], iv: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    if key.len() != 32 {
        return Err(CryptoError::LibraryFailure);
    }

    let iv = iv.unwrap_or(&[0u8; AES_BLOCK_SIZE]);

    if iv.len() != AES_BLOCK_SIZE {
        return Err(CryptoError::LibraryFailure);
    }

    let in_len = match c_uint::try_from(data.len()) {
        Ok(in_len) => in_len,
        _ => return Err(CryptoError::LibraryFailure),
    };

    if data.len() % AES_BLOCK_SIZE != 0 {
        return Err(CryptoError::LibraryFailure);
    }

    let slot = Slot::internal()?;

    let sym_key = unsafe {
        PK11_ImportSymKey(
            *slot,
            CKM_AES_CBC,
            PK11Origin::PK11_OriginUnwrap,
            CKA_ENCRYPT,
            SECItemBorrowed::wrap(key).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };

    let mut params = SECItemBorrowed::wrap(iv);
    let params_ptr: *mut SECItem = params.as_mut();
    let mut out_len: c_uint = 0;
    let mut out = vec![0; data.len()];
    unsafe {
        PK11_Decrypt(
            *sym_key,
            CKM_AES_CBC,
            params_ptr,
            out.as_mut_ptr(),
            &mut out_len,
            in_len,
            data.as_ptr(),
            in_len,
        )
        .into_result()?
    }
    // CKM_AES_CBC should have output length equal to input length.
    debug_assert_eq!(out_len, in_len);

    Ok(out)
}

/// Textbook HMAC-SHA256
pub fn hmac_sha256(key: &[u8], data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    let data_len = match u32::try_from(data.len()) {
        Ok(data_len) => data_len,
        _ => return Err(CryptoError::LibraryFailure),
    };

    let slot = Slot::internal()?;
    let sym_key = unsafe {
        PK11_ImportSymKey(
            *slot,
            CKM_SHA256_HMAC,
            PK11Origin::PK11_OriginUnwrap,
            CKA_SIGN,
            SECItemBorrowed::wrap(key).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };
    let param = SECItemBorrowed::make_empty();
    let context = unsafe {
        PK11_CreateContextBySymKey(CKM_SHA256_HMAC, CKA_SIGN, *sym_key, param.as_ref())
            .into_result()?
    };
    unsafe { PK11_DigestOp(*context, data.as_ptr(), data_len).into_result()? };
    let mut digest = vec![0u8; SHA256_LENGTH];
    let mut digest_len = 0u32;
    unsafe {
        PK11_DigestFinal(
            *context,
            digest.as_mut_ptr(),
            &mut digest_len,
            digest.len() as u32,
        )
        .into_result()?
    }
    assert_eq!(digest_len as usize, SHA256_LENGTH);
    Ok(digest)
}

/// Textbook SHA256
pub fn sha256(data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    let data_len: i32 = match i32::try_from(data.len()) {
        Ok(data_len) => data_len,
        _ => return Err(CryptoError::LibraryFailure),
    };
    let mut digest = vec![0u8; SHA256_LENGTH];
    unsafe {
        PK11_HashBuf(
            SECOidTag::SEC_OID_SHA256,
            digest.as_mut_ptr(),
            data.as_ptr(),
            data_len,
        )
        .into_result()?
    };
    Ok(digest)
}

pub fn random_bytes(count: usize) -> Result<Vec<u8>> {
    let count_cint: c_int = match c_int::try_from(count) {
        Ok(c) => c,
        _ => return Err(CryptoError::LibraryFailure),
    };

    let mut out = vec![0u8; count];
    unsafe { PK11_GenerateRandom(out.as_mut_ptr(), count_cint).into_result()? };
    Ok(out)
}

#[cfg(test)]
pub fn test_ecdh_p256_raw(
    peer_spki: &[u8],
    client_public_x: &[u8],
    client_public_y: &[u8],
    client_private: &[u8],
) -> Result<Vec<u8>> {
    nss_gk_api::init();

    let peer_public = nss_public_key_from_der_spki(peer_spki)?;

    /* NSS has no mechanism to import a raw elliptic curve coordinate as a private key.
     * We need to encode it in a key storage format such as PKCS#8. To avoid a dependency
     * on an ASN.1 encoder for this test, we'll do it manually. */
    let pkcs8_private_key_info_version = &[0x02, 0x01, 0x00];
    let rfc5915_ec_private_key_version = &[0x02, 0x01, 0x01];

    let (curve_oid, seq_len, alg_len, attr_len, ecpriv_len, param_len, spk_len) = (
        DER_OID_P256_BYTES,
        [0x81, 0x87].as_slice(),
        [0x13].as_slice(),
        [0x6d].as_slice(),
        [0x6b].as_slice(),
        [0x44].as_slice(),
        [0x42].as_slice(),
    );

    let priv_len = client_private.len() as u8; // < 127

    let mut pkcs8_priv: Vec<u8> = vec![];
    // RFC 5208 PrivateKeyInfo
    pkcs8_priv.push(0x30);
    pkcs8_priv.extend_from_slice(seq_len);
    //      Integer (0)
    pkcs8_priv.extend_from_slice(pkcs8_private_key_info_version);
    //      AlgorithmIdentifier
    pkcs8_priv.push(0x30);
    pkcs8_priv.extend_from_slice(alg_len);
    //          ObjectIdentifier
    pkcs8_priv.extend_from_slice(DER_OID_EC_PUBLIC_KEY_BYTES);
    //          RFC 5480 ECParameters
    pkcs8_priv.extend_from_slice(curve_oid);
    //      Attributes
    pkcs8_priv.push(0x04);
    pkcs8_priv.extend_from_slice(attr_len);
    //      RFC 5915 ECPrivateKey
    pkcs8_priv.push(0x30);
    pkcs8_priv.extend_from_slice(ecpriv_len);
    pkcs8_priv.extend_from_slice(rfc5915_ec_private_key_version);
    pkcs8_priv.push(0x04);
    pkcs8_priv.push(priv_len);
    pkcs8_priv.extend_from_slice(client_private);
    pkcs8_priv.push(0xa1);
    pkcs8_priv.extend_from_slice(param_len);
    pkcs8_priv.push(0x03);
    pkcs8_priv.extend_from_slice(spk_len);
    pkcs8_priv.push(0x0);
    pkcs8_priv.push(0x04); // SEC 1 encoded uncompressed point
    pkcs8_priv.extend_from_slice(client_public_x);
    pkcs8_priv.extend_from_slice(client_public_y);

    // Now we can import the private key.
    let slot = Slot::internal()?;
    let mut pkcs8_priv_item = SECItemBorrowed::wrap(&pkcs8_priv);
    let pkcs8_priv_item_ptr: *mut SECItem = pkcs8_priv_item.as_mut();
    let mut client_private_ptr = ptr::null_mut();
    unsafe {
        PK11_ImportDERPrivateKeyInfoAndReturnKey(
            *slot,
            pkcs8_priv_item_ptr,
            ptr::null_mut(),
            ptr::null_mut(),
            PR_FALSE,
            PR_FALSE,
            255, /* todo: expose KU_ flags in nss-gk-api */
            &mut client_private_ptr,
            ptr::null_mut(),
        )
    };
    let client_private = unsafe { PrivateKey::from_ptr(client_private_ptr) }?;

    let shared_point = ecdh_nss_raw(client_private, peer_public)?;

    Ok(shared_point)
}
