use super::CryptoError;
use nss_gk_api::p11::{
    PK11Origin, PK11_CreateContextBySymKey, PK11_Decrypt, PK11_DigestFinal, PK11_DigestOp,
    PK11_Encrypt, PK11_ExportDERPrivateKeyInfo, PK11_GenerateKeyPairWithOpFlags,
    PK11_GenerateRandom, PK11_HashBuf, PK11_ImportDERPrivateKeyInfoAndReturnKey, PK11_ImportSymKey,
    PK11_PubDeriveWithKDF, PK11_SignWithMechanism, PrivateKey, PublicKey,
    SECKEY_DecodeDERSubjectPublicKeyInfo, SECKEY_ExtractPublicKey, SECOidTag, Slot,
    SubjectPublicKeyInfo, AES_BLOCK_SIZE, PK11_ATTR_EXTRACTABLE, PK11_ATTR_INSENSITIVE,
    PK11_ATTR_SESSION, SHA256_LENGTH,
};
use nss_gk_api::{IntoResult, SECItem, SECItemBorrowed, ScopedSECItem, PR_FALSE};
use pkcs11_bindings::{
    CKA_DERIVE, CKA_ENCRYPT, CKA_SIGN, CKD_NULL, CKF_DERIVE, CKM_AES_CBC, CKM_ECDH1_DERIVE,
    CKM_ECDSA_SHA256, CKM_EC_KEY_PAIR_GEN, CKM_SHA256_HMAC, CKM_SHA512_HMAC,
};
use std::convert::TryFrom;
use std::os::raw::{c_int, c_uint};
use std::ptr;

use super::der;

#[cfg(test)]
use nss_gk_api::p11::PK11_VerifyWithMechanism;

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

fn generate_p256_nss() -> Result<(PrivateKey, PublicKey)> {
    // Hard-coding the P256 OID here is easier than extracting a group name from peer_public and
    // comparing it with P256. We'll fail in `PK11_GenerateKeyPairWithOpFlags` if peer_public is on
    // the wrong curve.
    let oid_bytes = der::object_id(der::OID_SECP256R1_BYTES)?;
    let mut oid = SECItemBorrowed::wrap(&oid_bytes);
    let oid_ptr: *mut SECItem = oid.as_mut();

    let slot = Slot::internal()?;

    let mut client_public_ptr = ptr::null_mut();

    // We have to be careful with error handling between the `PK11_GenerateKeyPairWithOpFlags` and
    // `PublicKey::from_ptr` calls here, so I've wrapped them in the same unsafe block as a
    // warning. TODO(jms) Replace this once there is a safer alternative.
    // https://github.com/mozilla/nss-gk-api/issues/1
    unsafe {
        let client_private =
            // Type of `param` argument depends on mechanism. For EC keygen it is
            // `SECKEYECParams *` which is a typedef for `SECItem *`.
            PK11_GenerateKeyPairWithOpFlags(
                *slot,
                CKM_EC_KEY_PAIR_GEN,
                oid_ptr.cast(),
                &mut client_public_ptr,
                PK11_ATTR_EXTRACTABLE | PK11_ATTR_INSENSITIVE | PK11_ATTR_SESSION,
                CKF_DERIVE,
                CKF_DERIVE,
                ptr::null_mut(),
            )
            .into_result()?;

        let client_public = PublicKey::from_ptr(client_public_ptr)?;

        Ok((client_private, client_public))
    }
}

/// This returns a PKCS#8 ECPrivateKey and an uncompressed SEC1 public key.
pub fn gen_p256() -> Result<(Vec<u8>, Vec<u8>)> {
    nss_gk_api::init();

    let (client_private, client_public) = generate_p256_nss()?;

    let pkcs8_priv = unsafe {
        let pkcs8_priv_item: ScopedSECItem =
            PK11_ExportDERPrivateKeyInfo(*client_private, ptr::null_mut()).into_result()?;
        pkcs8_priv_item.into_vec()
    };

    let sec1_pub = client_public.key_data()?;

    Ok((pkcs8_priv, sec1_pub))
}

pub fn ecdsa_p256_sha256_sign_raw(private: &[u8], data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    let slot = Slot::internal()?;

    let imported_private: PrivateKey = unsafe {
        let mut imported_private_ptr = ptr::null_mut();
        PK11_ImportDERPrivateKeyInfoAndReturnKey(
            *slot,
            SECItemBorrowed::wrap(private).as_mut(),
            ptr::null_mut(),
            ptr::null_mut(),
            PR_FALSE,
            PR_FALSE,
            255, /* todo: expose KU_ flags in nss-gk-api */
            &mut imported_private_ptr,
            ptr::null_mut(),
        );
        imported_private_ptr.into_result()?
    };

    let signature_buf = vec![0; 64];
    unsafe {
        PK11_SignWithMechanism(
            *imported_private,
            CKM_ECDSA_SHA256,
            ptr::null_mut(),
            SECItemBorrowed::wrap(&signature_buf).as_mut(),
            SECItemBorrowed::wrap(data).as_mut(),
        )
        .into_result()?;
    }

    let (r, s) = signature_buf.split_at(32);
    der::sequence(&[&der::integer(r)?, &der::integer(s)?])
}

/// Ephemeral ECDH over P256. Takes a DER SubjectPublicKeyInfo that encodes a public key. Generates
/// an ephemeral P256 key pair. Returns
///  1) the x coordinate of the shared point, and
///  2) the uncompressed SEC 1 encoding of the ephemeral public key.
pub fn ecdhe_p256_raw(peer_spki: &[u8]) -> Result<(Vec<u8>, Vec<u8>)> {
    nss_gk_api::init();

    let peer_public = nss_public_key_from_der_spki(peer_spki)?;

    let (client_private, client_public) = generate_p256_nss()?;

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
    nss_gk_api::init();

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

    // NSS has no mechanism to import a raw elliptic curve coordinate as a private key.
    // We need to encode it in an RFC 5208 PrivateKeyInfo:
    //
    //   PrivateKeyInfo ::= SEQUENCE {
    //     version                   Version,
    //     privateKeyAlgorithm       PrivateKeyAlgorithmIdentifier,
    //     privateKey                PrivateKey,
    //     attributes           [0]  IMPLICIT Attributes OPTIONAL }
    //
    //    Version ::= INTEGER
    //    PrivateKeyAlgorithmIdentifier ::= AlgorithmIdentifier
    //    PrivateKey ::= OCTET STRING
    //    Attributes ::= SET OF Attribute
    //
    // The privateKey field will contain an RFC 5915 ECPrivateKey:
    //   ECPrivateKey ::= SEQUENCE {
    //     version        INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
    //     privateKey     OCTET STRING,
    //     parameters [0] ECParameters {{ NamedCurve }} OPTIONAL,
    //     publicKey  [1] BIT STRING OPTIONAL
    //   }

    // PrivateKeyInfo
    let priv_key_info = der::sequence(&[
        // version
        &der::integer(&[0x00])?,
        // privateKeyAlgorithm
        &der::sequence(&[
            &der::object_id(der::OID_EC_PUBLIC_KEY_BYTES)?,
            &der::object_id(der::OID_SECP256R1_BYTES)?,
        ])?,
        // privateKey
        &der::octet_string(
            // ECPrivateKey
            &der::sequence(&[
                // version
                &der::integer(&[0x01])?,
                // privateKey
                &der::octet_string(client_private)?,
                // publicKey
                &der::context_specific_explicit_tag(
                    1, // publicKey
                    &der::bit_string(&[&[0x04], client_public_x, client_public_y].concat())?,
                )?,
            ])?,
        )?,
    ])?;

    // Now we can import the private key.
    let slot = Slot::internal()?;
    let mut priv_key_info_item = SECItemBorrowed::wrap(&priv_key_info);
    let priv_key_info_item_ptr: *mut SECItem = priv_key_info_item.as_mut();
    let mut client_private_ptr = ptr::null_mut();
    unsafe {
        PK11_ImportDERPrivateKeyInfoAndReturnKey(
            *slot,
            priv_key_info_item_ptr,
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

#[cfg(test)]
pub fn test_ecdsa_p256_sha256_verify_raw(
    public: &[u8],
    signature: &[u8],
    data: &[u8],
) -> Result<()> {
    nss_gk_api::init();

    let signature = der::read_p256_sig(signature)?;
    let public = nss_public_key_from_der_spki(public)?;
    unsafe {
        PK11_VerifyWithMechanism(
            *public,
            CKM_ECDSA_SHA256,
            ptr::null_mut(),
            SECItemBorrowed::wrap(&signature).as_mut(),
            SECItemBorrowed::wrap(data).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    }
    Ok(())
}
