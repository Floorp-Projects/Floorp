use super::{COSEAlgorithm, COSEEC2Key, COSEKey, COSEKeyType, ECDHSecret, ECDSACurve};
use nss_gk_api::p11::{
    PK11Origin, PK11_CreateContextBySymKey, PK11_Decrypt, PK11_DigestFinal, PK11_DigestOp,
    PK11_Encrypt, PK11_GenerateKeyPairWithOpFlags, PK11_HashBuf, PK11_ImportSymKey,
    PK11_PubDeriveWithKDF, PrivateKey, PublicKey, SECKEY_DecodeDERSubjectPublicKeyInfo,
    SECKEY_ExtractPublicKey, SECOidTag, Slot, SubjectPublicKeyInfo, AES_BLOCK_SIZE,
    PK11_ATTR_SESSION, SHA256_LENGTH,
};
use nss_gk_api::{Error as NSSError, IntoResult, SECItem, SECItemBorrowed, PR_FALSE};
use pkcs11_bindings::{
    CKA_DERIVE, CKA_ENCRYPT, CKA_SIGN, CKD_NULL, CKF_DERIVE, CKM_AES_CBC, CKM_ECDH1_DERIVE,
    CKM_EC_KEY_PAIR_GEN, CKM_SHA256_HMAC, CKM_SHA512_HMAC,
};
use serde::Serialize;
use serde_bytes::ByteBuf;
use std::convert::{TryFrom, TryInto};
use std::num::TryFromIntError;
use std::os::raw::c_uint;
use std::ptr;

#[cfg(test)]
use nss_gk_api::p11::{PK11_ImportDERPrivateKeyInfoAndReturnKey, SECKEY_ConvertToPublicKey};

/// Errors that can be returned from COSE functions.
#[derive(Clone, Debug, Serialize)]
pub enum BackendError {
    NSSError(String),
    TryFromError,
    UnsupportedAlgorithm(COSEAlgorithm),
    UnsupportedCurve(ECDSACurve),
    UnsupportedKeyType,
}

impl From<NSSError> for BackendError {
    fn from(e: NSSError) -> Self {
        BackendError::NSSError(format!("{}", e))
    }
}

impl From<TryFromIntError> for BackendError {
    fn from(_: TryFromIntError) -> Self {
        BackendError::TryFromError
    }
}

pub type Result<T> = std::result::Result<T, BackendError>;

// Object identifiers in DER tag-length-value form

const DER_OID_EC_PUBLIC_KEY_BYTES: &[u8] = &[
    0x06, 0x07,
    /* {iso(1) member-body(2) us(840) ansi-x962(10045) keyType(2) ecPublicKey(1)} */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
];

const DER_OID_P256_BYTES: &[u8] = &[
    0x06, 0x08,
    /* {iso(1) member-body(2) us(840) ansi-x962(10045) curves(3) prime(1) prime256v1(7)} */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
];
const DER_OID_P384_BYTES: &[u8] = &[
    0x06, 0x05,
    /* {iso(1) identified-organization(3) certicom(132) curve(0) ansip384r1(34)} */
    0x2b, 0x81, 0x04, 0x00, 0x22,
];
const DER_OID_P521_BYTES: &[u8] = &[
    0x06, 0x05,
    /* {iso(1) identified-organization(3) certicom(132) curve(0) ansip521r1(35)} */
    0x2b, 0x81, 0x04, 0x00, 0x23,
];

/* From CTAP2.1 spec:

initialize()

    This is run by the platform when starting a series of transactions with a specific authenticator.
encapsulate(peerCoseKey) → (coseKey, sharedSecret) | error

    Generates an encapsulation for the authenticator’s public key and returns the message to transmit and the shared secret.
encrypt(key, demPlaintext) → ciphertext

    Encrypts a plaintext to produce a ciphertext, which may be longer than the plaintext. The plaintext is restricted to being a multiple of the AES block size (16 bytes) in length.
decrypt(key, ciphertext) → plaintext | error

    Decrypts a ciphertext and returns the plaintext.
authenticate(key, message) → signature

    Computes a MAC of the given message.
*/

// TODO(MS): Maybe remove ByteBuf and return Vec<u8>'s instead for a cleaner interface
pub(crate) fn serialize_key(_curve: ECDSACurve, key: &[u8]) -> Result<(ByteBuf, ByteBuf)> {
    // TODO(MS): I actually have NO idea how to do this with NSS
    let length = key[1..].len() / 2;
    let chunks: Vec<_> = key[1..].chunks_exact(length).collect();
    Ok((
        ByteBuf::from(chunks[0].to_vec()),
        ByteBuf::from(chunks[1].to_vec()),
    ))
}

pub(crate) fn parse_key(_curve: ECDSACurve, x: &[u8], y: &[u8]) -> Result<Vec<u8>> {
    if x.len() != y.len() {
        return Err(BackendError::NSSError(
            "EC coordinates not equally long".to_string(),
        ));
    }
    let mut buf = Vec::with_capacity(2 * x.len() + 1);
    // The uncompressed point format is defined in Section 2.3.3 of "SEC 1: Elliptic Curve
    // Cryptography" https://www.secg.org/sec1-v2.pdf.
    buf.push(0x04);
    buf.extend_from_slice(x);
    buf.extend_from_slice(y);
    Ok(buf)
}

fn der_spki_from_cose(cose_key: &COSEKey) -> Result<Vec<u8>> {
    let ec2key = match cose_key.key {
        COSEKeyType::EC2(ref ec2key) => ec2key,
        _ => return Err(BackendError::UnsupportedKeyType),
    };

    let (curve_oid, seq_len, alg_len, spk_len) = match ec2key.curve {
        ECDSACurve::SECP256R1 => (
            DER_OID_P256_BYTES,
            [0x59].as_slice(),
            [0x13].as_slice(),
            [0x42].as_slice(),
        ),
        ECDSACurve::SECP384R1 => (
            DER_OID_P384_BYTES,
            [0x76].as_slice(),
            [0x10].as_slice(),
            [0x62].as_slice(),
        ),
        ECDSACurve::SECP521R1 => (
            DER_OID_P521_BYTES,
            [0x81, 0x9b].as_slice(),
            [0x10].as_slice(),
            [0x8a, 0xdf].as_slice(),
        ),
        x => return Err(BackendError::UnsupportedCurve(x)),
    };

    let cose_key_sec1 = parse_key(ec2key.curve, &ec2key.x, &ec2key.y)?;

    // [RFC 5280]
    let mut spki: Vec<u8> = vec![];
    // SubjectPublicKeyInfo
    spki.push(0x30);
    spki.extend_from_slice(seq_len);
    //      AlgorithmIdentifier
    spki.push(0x30);
    spki.extend_from_slice(alg_len);
    //          ObjectIdentifier
    spki.extend_from_slice(DER_OID_EC_PUBLIC_KEY_BYTES);
    //          RFC 5480 ECParameters
    spki.extend_from_slice(curve_oid);
    //      BIT STRING encoding uncompressed SEC1 public point
    spki.push(0x03);
    spki.extend_from_slice(spk_len);
    spki.push(0x0); // no trailing zeros
    spki.extend_from_slice(&cose_key_sec1);

    Ok(spki)
}

/// This is run by the platform when starting a series of transactions with a specific authenticator.
//pub(crate) fn initialize() { }

/// Generates an encapsulation for the authenticator's public key and returns the message
/// to transmit and the shared secret.
///
/// `peer_cose_key` is the authenticator's (peer's) public key.
pub(crate) fn encapsulate(peer_cose_key: &COSEKey) -> Result<ECDHSecret> {
    nss_gk_api::init();
    // Generate an ephmeral keypair to do ECDH with the authenticator.
    // This is "platformKeyAgreementKey".
    let ec2key = match peer_cose_key.key {
        COSEKeyType::EC2(ref ec2key) => ec2key,
        _ => return Err(BackendError::UnsupportedKeyType),
    };

    let mut oid = match ec2key.curve {
        ECDSACurve::SECP256R1 => SECItemBorrowed::wrap(DER_OID_P256_BYTES),
        ECDSACurve::SECP384R1 => SECItemBorrowed::wrap(DER_OID_P384_BYTES),
        ECDSACurve::SECP521R1 => SECItemBorrowed::wrap(DER_OID_P521_BYTES),
        x => return Err(BackendError::UnsupportedCurve(x)),
    };
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

    let peer_spki = der_spki_from_cose(peer_cose_key)?;
    let peer_public = nss_public_key_from_der_spki(&peer_spki)?;
    let shared_secret = encapsulate_helper(peer_public, client_private)?;

    let client_cose_key = cose_key_from_nss_public(peer_cose_key.alg, ec2key.curve, client_public)?;

    Ok(ECDHSecret {
        remote: COSEKey {
            alg: peer_cose_key.alg,
            key: COSEKeyType::EC2(ec2key.clone()),
        },
        my: client_cose_key,
        shared_secret,
    })
}

fn nss_public_key_from_der_spki(spki: &[u8]) -> Result<PublicKey> {
    let mut spki_item = SECItemBorrowed::wrap(&spki);
    let spki_item_ptr: *mut SECItem = spki_item.as_mut();
    let nss_spki = unsafe {
        SubjectPublicKeyInfo::from_ptr(SECKEY_DecodeDERSubjectPublicKeyInfo(spki_item_ptr))?
    };
    let public_key = unsafe { PublicKey::from_ptr(SECKEY_ExtractPublicKey(*nss_spki))? };
    Ok(public_key)
}

fn cose_key_from_nss_public(
    alg: COSEAlgorithm,
    curve: ECDSACurve,
    nss_public: PublicKey,
) -> Result<COSEKey> {
    let public_data = nss_public.key_data()?;
    let (public_x, public_y) = serialize_key(curve, &public_data)?;
    Ok(COSEKey {
        alg,
        key: COSEKeyType::EC2(COSEEC2Key {
            curve,
            x: public_x.to_vec(),
            y: public_y.to_vec(),
        }),
    })
}

/// `peer_public`: The authenticator's public key.
/// `client_private`: Our ephemeral private key.
fn encapsulate_helper(peer_public: PublicKey, client_private: PrivateKey) -> Result<Vec<u8>> {
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
    let mut shared_secret = [0u8; SHA256_LENGTH];
    unsafe {
        PK11_HashBuf(
            SECOidTag::SEC_OID_SHA256,
            shared_secret.as_mut_ptr(),
            ecdh_x_coord_bytes.as_ptr(),
            ecdh_x_coord_bytes.len() as i32,
        )
    };
    Ok(shared_secret.to_vec())
}

/// Encrypts a plaintext to produce a ciphertext, which may be longer than the plaintext.
/// The plaintext is restricted to being a multiple of the AES block size (16 bytes) in length.
pub(crate) fn encrypt(key: &[u8], data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();

    if key.len() != 32 {
        return Err(BackendError::NSSError(
            "Invalid AES-256 key length".to_string(),
        ));
    }

    // The input must be a multiple of the AES block size, 16
    if data.len() % AES_BLOCK_SIZE != 0 {
        return Err(BackendError::NSSError(
            "Input to encrypt is too long".to_string(),
        ));
    }
    let in_len = c_uint::try_from(data.len())?;

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

    let iv = [0u8; AES_BLOCK_SIZE];
    let mut params = SECItemBorrowed::wrap(&iv);
    let params_ptr: *mut SECItem = params.as_mut();
    let mut out_len: c_uint = 0;
    let mut out = vec![0; in_len as usize];
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

/// Decrypts a ciphertext and returns the plaintext.
pub(crate) fn decrypt(key: &[u8], data: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();
    let slot = Slot::internal()?;

    if key.len() != 32 {
        return Err(BackendError::NSSError(
            "Invalid AES-256 key length".to_string(),
        ));
    }

    // The input must be a multiple of the AES block size, 16
    if data.len() % AES_BLOCK_SIZE != 0 {
        return Err(BackendError::NSSError(
            "Invalid input to decrypt".to_string(),
        ));
    }
    let in_len = c_uint::try_from(data.len())?;

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

    let iv = [0u8; AES_BLOCK_SIZE];
    let mut params = SECItemBorrowed::wrap(&iv);
    let params_ptr: *mut SECItem = params.as_mut();
    let mut out_len: c_uint = 0;
    let mut out = vec![0; in_len as usize];
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

/// Computes a MAC of the given message.
pub(crate) fn authenticate(token: &[u8], input: &[u8]) -> Result<Vec<u8>> {
    nss_gk_api::init();
    let slot = Slot::internal()?;
    let sym_key = unsafe {
        PK11_ImportSymKey(
            *slot,
            CKM_SHA256_HMAC,
            PK11Origin::PK11_OriginUnwrap,
            CKA_SIGN,
            SECItemBorrowed::wrap(token).as_mut(),
            ptr::null_mut(),
        )
        .into_result()?
    };
    let param = SECItemBorrowed::make_empty();
    let context = unsafe {
        PK11_CreateContextBySymKey(CKM_SHA256_HMAC, CKA_SIGN, *sym_key, param.as_ref())
            .into_result()?
    };
    unsafe { PK11_DigestOp(*context, input.as_ptr(), input.len().try_into()?).into_result()? };
    let mut digest = vec![0u8; 32];
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
    assert_eq!(digest_len, 32);
    Ok(digest)
}

#[cfg(test)]
pub(crate) fn test_encapsulate(
    peer_coseec2_key: &COSEEC2Key,
    alg: COSEAlgorithm,
    my_pub_key: &[u8],
    my_priv_key: &[u8],
) -> Result<ECDHSecret> {
    nss_gk_api::init();

    let peer_cose_key = COSEKey {
        alg: alg,
        key: COSEKeyType::EC2(peer_coseec2_key.clone()),
    };
    let spki = der_spki_from_cose(&peer_cose_key)?;
    let peer_public = nss_public_key_from_der_spki(&spki)?;

    /* NSS has no mechanism to import a raw elliptic curve coordinate as a private key.
     * We need to encode it in a key storage format such as PKCS#8. To avoid a dependency
     * on an ASN.1 encoder for this test, we'll do it manually. */
    let pkcs8_private_key_info_version = &[0x02, 0x01, 0x00];
    let rfc5915_ec_private_key_version = &[0x02, 0x01, 0x01];

    let (curve_oid, seq_len, alg_len, attr_len, ecpriv_len, param_len, spk_len) =
        match peer_coseec2_key.curve {
            ECDSACurve::SECP256R1 => (
                DER_OID_P256_BYTES,
                [0x81, 0x87].as_slice(),
                [0x13].as_slice(),
                [0x6d].as_slice(),
                [0x6b].as_slice(),
                [0x44].as_slice(),
                [0x42].as_slice(),
            ),
            x => return Err(BackendError::UnsupportedCurve(x)),
        };

    let priv_len = my_priv_key.len() as u8; // < 127

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
    pkcs8_priv.extend_from_slice(DER_OID_P256_BYTES);
    //      Attributes
    pkcs8_priv.push(0x04);
    pkcs8_priv.extend_from_slice(attr_len);
    //      RFC 5915 ECPrivateKey
    pkcs8_priv.push(0x30);
    pkcs8_priv.extend_from_slice(ecpriv_len);
    pkcs8_priv.extend_from_slice(rfc5915_ec_private_key_version);
    pkcs8_priv.push(0x04);
    pkcs8_priv.push(priv_len);
    pkcs8_priv.extend_from_slice(my_priv_key);
    pkcs8_priv.push(0xa1);
    pkcs8_priv.extend_from_slice(param_len);
    pkcs8_priv.push(0x03);
    pkcs8_priv.extend_from_slice(spk_len);
    pkcs8_priv.push(0x0);
    pkcs8_priv.extend_from_slice(&my_pub_key);

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
    let client_public = unsafe { PublicKey::from_ptr(SECKEY_ConvertToPublicKey(*client_private))? };
    let client_cose_key = cose_key_from_nss_public(alg, peer_coseec2_key.curve, client_public)?;

    let shared_secret = encapsulate_helper(peer_public, client_private)?;

    Ok(ECDHSecret {
        remote: peer_cose_key,
        my: client_cose_key,
        shared_secret,
    })
}
