use super::CryptoError;
use openssl::bn::BigNumContext;
use openssl::derive::Deriver;
use openssl::ec::{EcGroup, EcKey, PointConversionForm};
use openssl::error::ErrorStack;
use openssl::hash::{hash, MessageDigest};
use openssl::nid::Nid;
use openssl::pkey::{PKey, Private, Public};
use openssl::rand::rand_bytes;
use openssl::sign::Signer;
use openssl::symm::{Cipher, Crypter, Mode};
use std::os::raw::c_int;

#[cfg(test)]
use openssl::ec::EcPoint;

#[cfg(test)]
use openssl::bn::BigNum;

const AES_BLOCK_SIZE: usize = 16;

impl From<ErrorStack> for CryptoError {
    fn from(e: ErrorStack) -> Self {
        CryptoError::Backend(format!("{e}"))
    }
}

impl From<&ErrorStack> for CryptoError {
    fn from(e: &ErrorStack) -> Self {
        CryptoError::Backend(format!("{e}"))
    }
}

pub type Result<T> = std::result::Result<T, CryptoError>;

/// ECDH using OpenSSL types. Computes the x coordinate of scalar multiplication of `peer_public`
/// by `client_private`.
fn ecdh_openssl_raw(client_private: EcKey<Private>, peer_public: EcKey<Public>) -> Result<Vec<u8>> {
    let client_pkey = PKey::from_ec_key(client_private)?;
    let peer_pkey = PKey::from_ec_key(peer_public)?;
    let mut deriver = Deriver::new(&client_pkey)?;
    deriver.set_peer(&peer_pkey)?;
    let shared_point = deriver.derive_to_vec()?;
    Ok(shared_point)
}

/// Ephemeral ECDH over P256. Takes a DER SubjectPublicKeyInfo that encodes a public key. Generates
/// an ephemeral P256 key pair. Returns
///  1) the x coordinate of the shared point, and
///  2) the uncompressed SEC 1 encoding of the ephemeral public key.
pub fn ecdhe_p256_raw(peer_spki: &[u8]) -> Result<(Vec<u8>, Vec<u8>)> {
    let peer_public = EcKey::public_key_from_der(peer_spki)?;

    // Hard-coding the P256 group here is easier than extracting a group name from peer_public and
    // comparing it with P256. We'll fail in key derivation if peer_public is on the wrong curve.
    let group = EcGroup::from_curve_name(Nid::X9_62_PRIME256V1)?;

    let mut bn_ctx = BigNumContext::new()?;
    let client_private = EcKey::generate(&group)?;
    let client_public_sec1 = client_private.public_key().to_bytes(
        &group,
        PointConversionForm::UNCOMPRESSED,
        &mut bn_ctx,
    )?;

    let shared_point = ecdh_openssl_raw(client_private, peer_public)?;

    Ok((shared_point, client_public_sec1))
}

/// AES-256-CBC encryption for data that is a multiple of the AES block size (16 bytes) in length.
/// Uses the zero IV if `iv` is None.
pub fn encrypt_aes_256_cbc_no_pad(key: &[u8], iv: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>> {
    let iv = iv.unwrap_or(&[0u8; AES_BLOCK_SIZE]);

    let mut encrypter = Crypter::new(Cipher::aes_256_cbc(), Mode::Encrypt, key, Some(iv))?;
    encrypter.pad(false);

    let in_len = data.len();
    if in_len % AES_BLOCK_SIZE != 0 {
        return Err(CryptoError::LibraryFailure);
    }

    // OpenSSL would panic if we didn't allocate an extra block here.
    let mut out = vec![0; in_len + AES_BLOCK_SIZE];
    let mut out_len = 0;
    out_len += encrypter.update(data, out.as_mut_slice())?;
    out_len += encrypter.finalize(out.as_mut_slice())?;
    debug_assert_eq!(in_len, out_len);

    out.truncate(out_len);
    Ok(out)
}

/// AES-256-CBC decryption for data that is a multiple of the AES block size (16 bytes) in length.
/// Uses the zero IV if `iv` is None.
pub fn decrypt_aes_256_cbc_no_pad(key: &[u8], iv: Option<&[u8]>, data: &[u8]) -> Result<Vec<u8>> {
    let iv = iv.unwrap_or(&[0u8; AES_BLOCK_SIZE]);

    let mut encrypter = Crypter::new(Cipher::aes_256_cbc(), Mode::Decrypt, key, Some(iv))?;
    encrypter.pad(false);

    let in_len = data.len();
    if in_len % AES_BLOCK_SIZE != 0 {
        return Err(CryptoError::LibraryFailure);
    }

    // OpenSSL would panic if we didn't allocate an extra block here.
    let mut out = vec![0; in_len + AES_BLOCK_SIZE];
    let mut out_len = 0;
    out_len += encrypter.update(data, out.as_mut_slice())?;
    out_len += encrypter.finalize(out.as_mut_slice())?;
    debug_assert_eq!(in_len, out_len);

    out.truncate(out_len);
    Ok(out)
}

/// Textbook HMAC-SHA256
pub fn hmac_sha256(key: &[u8], data: &[u8]) -> Result<Vec<u8>> {
    let key = PKey::hmac(key)?;
    let mut signer = Signer::new(MessageDigest::sha256(), &key)?;
    signer.update(data)?;
    Ok(signer.sign_to_vec()?)
}

pub fn sha256(data: &[u8]) -> Result<Vec<u8>> {
    let digest = hash(MessageDigest::sha256(), data)?;
    Ok(digest.as_ref().to_vec())
}

pub fn random_bytes(count: usize) -> Result<Vec<u8>> {
    if count > c_int::MAX as usize {
        return Err(CryptoError::LibraryFailure);
    }
    let mut out = vec![0u8; count];
    rand_bytes(&mut out)?;
    Ok(out)
}

#[cfg(test)]
pub fn test_ecdh_p256_raw(
    peer_spki: &[u8],
    client_public_x: &[u8],
    client_public_y: &[u8],
    client_private: &[u8],
) -> Result<Vec<u8>> {
    let peer_public = EcKey::public_key_from_der(peer_spki)?;
    let group = peer_public.group();

    let mut client_pub_sec1 = vec![];
    client_pub_sec1.push(0x04); // SEC 1 encoded uncompressed point
    client_pub_sec1.extend_from_slice(&client_public_x);
    client_pub_sec1.extend_from_slice(&client_public_y);

    let mut ctx = BigNumContext::new()?;
    let client_pub_point = EcPoint::from_bytes(&group, &client_pub_sec1, &mut ctx)?;
    let client_priv_bignum = BigNum::from_slice(client_private)?;
    let client_private =
        EcKey::from_private_components(&group, &client_priv_bignum, &client_pub_point)?;

    let shared_point = ecdh_openssl_raw(client_private, peer_public)?;

    Ok(shared_point)
}

pub fn gen_p256() -> Result<(Vec<u8>, Vec<u8>)> {
    unimplemented!()
}

pub fn ecdsa_p256_sha256_sign_raw(_private: &[u8], _data: &[u8]) -> Result<Vec<u8>> {
    unimplemented!()
}

#[allow(dead_code)]
#[cfg(test)]
pub fn test_ecdsa_p256_sha256_verify_raw(
    _public: &[u8],
    _signature: &[u8],
    _data: &[u8],
) -> Result<()> {
    unimplemented!()
}
