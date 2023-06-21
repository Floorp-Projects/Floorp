// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

//! Utilities for ECIES encryption / decryption used by the Prio client and server.

use crate::prng::PrngError;

use aes_gcm::aead::generic_array::typenum::U16;
use aes_gcm::aead::generic_array::GenericArray;
use aes_gcm::{AeadInPlace, NewAead};
use ring::agreement;
type Aes128 = aes_gcm::AesGcm<aes_gcm::aes::Aes128, U16>;

/// Length of the EC public key (X9.62 format)
pub const PUBLICKEY_LENGTH: usize = 65;
/// Length of the AES-GCM tag
pub const TAG_LENGTH: usize = 16;
/// Length of the symmetric AES-GCM key
const KEY_LENGTH: usize = 16;

/// Possible errors from encryption / decryption.
#[derive(Debug, thiserror::Error)]
pub enum EncryptError {
    /// Base64 decoding error
    #[error("base64 decoding error")]
    DecodeBase64(#[from] base64::DecodeError),
    /// Error in ECDH
    #[error("error in ECDH")]
    KeyAgreement,
    /// Buffer for ciphertext was not large enough
    #[error("buffer for ciphertext was not large enough")]
    Encryption,
    /// Authentication tags did not match.
    #[error("authentication tags did not match")]
    Decryption,
    /// Input ciphertext was too small
    #[error("input ciphertext was too small")]
    DecryptionLength,
    /// PRNG error
    #[error("prng error: {0}")]
    Prng(#[from] PrngError),
    /// failure when calling getrandom().
    #[error("getrandom: {0}")]
    GetRandom(#[from] getrandom::Error),
}

/// NIST P-256, public key in X9.62 uncompressed format
#[derive(Debug, Clone)]
pub struct PublicKey(Vec<u8>);

/// NIST P-256, private key
///
/// X9.62 uncompressed public key concatenated with the secret scalar.
#[derive(Debug, Clone)]
pub struct PrivateKey(Vec<u8>);

impl PublicKey {
    /// Load public key from a base64 encoded X9.62 uncompressed representation.
    pub fn from_base64(key: &str) -> Result<Self, EncryptError> {
        let keydata = base64::decode(key)?;
        Ok(PublicKey(keydata))
    }
}

/// Copy public key from a private key
impl std::convert::From<&PrivateKey> for PublicKey {
    fn from(pk: &PrivateKey) -> Self {
        PublicKey(pk.0[..PUBLICKEY_LENGTH].to_owned())
    }
}

impl PrivateKey {
    /// Load private key from a base64 encoded string.
    pub fn from_base64(key: &str) -> Result<Self, EncryptError> {
        let keydata = base64::decode(key)?;
        Ok(PrivateKey(keydata))
    }
}

/// Encrypt a bytestring using the public key
///
/// This uses ECIES with X9.63 key derivation function and AES-GCM for the
/// symmetic encryption and MAC.
pub fn encrypt_share(share: &[u8], key: &PublicKey) -> Result<Vec<u8>, EncryptError> {
    let rng = ring::rand::SystemRandom::new();
    let ephemeral_priv = agreement::EphemeralPrivateKey::generate(&agreement::ECDH_P256, &rng)
        .map_err(|_| EncryptError::KeyAgreement)?;
    let peer_public = agreement::UnparsedPublicKey::new(&agreement::ECDH_P256, &key.0);
    let ephemeral_pub = ephemeral_priv
        .compute_public_key()
        .map_err(|_| EncryptError::KeyAgreement)?;

    let symmetric_key_bytes = agreement::agree_ephemeral(
        ephemeral_priv,
        &peer_public,
        EncryptError::KeyAgreement,
        |material| Ok(x963_kdf(material, ephemeral_pub.as_ref())),
    )?;

    let in_out = share.to_owned();
    let encrypted = encrypt_aes_gcm(
        &symmetric_key_bytes[..16],
        &symmetric_key_bytes[16..],
        in_out,
    )?;

    let mut output = Vec::with_capacity(encrypted.len() + ephemeral_pub.as_ref().len());
    output.extend_from_slice(ephemeral_pub.as_ref());
    output.extend_from_slice(&encrypted);

    Ok(output)
}

/// Decrypt a bytestring using the private key
///
/// This uses ECIES with X9.63 key derivation function and AES-GCM for the
/// symmetic encryption and MAC.
pub fn decrypt_share(share: &[u8], key: &PrivateKey) -> Result<Vec<u8>, EncryptError> {
    if share.len() < PUBLICKEY_LENGTH + TAG_LENGTH {
        return Err(EncryptError::DecryptionLength);
    }
    let empheral_pub_bytes: &[u8] = &share[0..PUBLICKEY_LENGTH];

    let ephemeral_pub =
        agreement::UnparsedPublicKey::new(&agreement::ECDH_P256, empheral_pub_bytes);

    let fake_rng = ring::test::rand::FixedSliceRandom {
        // private key consists of the public key + private scalar
        bytes: &key.0[PUBLICKEY_LENGTH..],
    };

    let private_key = agreement::EphemeralPrivateKey::generate(&agreement::ECDH_P256, &fake_rng)
        .map_err(|_| EncryptError::KeyAgreement)?;

    let symmetric_key_bytes = agreement::agree_ephemeral(
        private_key,
        &ephemeral_pub,
        EncryptError::KeyAgreement,
        |material| Ok(x963_kdf(material, empheral_pub_bytes)),
    )?;

    // in_out is the AES-GCM ciphertext+tag, wihtout the ephemeral EC pubkey
    let in_out = share[PUBLICKEY_LENGTH..].to_owned();
    decrypt_aes_gcm(
        &symmetric_key_bytes[..KEY_LENGTH],
        &symmetric_key_bytes[KEY_LENGTH..],
        in_out,
    )
}

fn x963_kdf(z: &[u8], shared_info: &[u8]) -> [u8; 32] {
    let mut hasher = ring::digest::Context::new(&ring::digest::SHA256);
    hasher.update(z);
    hasher.update(&1u32.to_be_bytes());
    hasher.update(shared_info);
    let digest = hasher.finish();
    use std::convert::TryInto;
    // unwrap never fails because SHA256 output len is 32
    digest.as_ref().try_into().unwrap()
}

fn decrypt_aes_gcm(key: &[u8], nonce: &[u8], mut data: Vec<u8>) -> Result<Vec<u8>, EncryptError> {
    let cipher = Aes128::new(GenericArray::from_slice(key));
    cipher
        .decrypt_in_place(GenericArray::from_slice(nonce), &[], &mut data)
        .map_err(|_| EncryptError::Decryption)?;
    Ok(data)
}

fn encrypt_aes_gcm(key: &[u8], nonce: &[u8], mut data: Vec<u8>) -> Result<Vec<u8>, EncryptError> {
    let cipher = Aes128::new(GenericArray::from_slice(key));
    cipher
        .encrypt_in_place(GenericArray::from_slice(nonce), &[], &mut data)
        .map_err(|_| EncryptError::Encryption)?;
    Ok(data)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encrypt_decrypt() -> Result<(), EncryptError> {
        let pub_key = PublicKey::from_base64(
            "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgNt9\
             HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVQ=",
        )?;
        let priv_key = PrivateKey::from_base64(
            "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOgN\
             t9HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVSq3TfyKwn0NrftKisKKVSaTOt5seJ67P5QL4hxgPWvxw==",
        )?;
        let data = (0..100).map(|_| rand::random::<u8>()).collect::<Vec<u8>>();
        let encrypted = encrypt_share(&data, &pub_key)?;

        let decrypted = decrypt_share(&encrypted, &priv_key)?;
        assert_eq!(decrypted, data);
        Ok(())
    }

    #[test]
    fn test_interop() {
        let share1 = base64::decode("Kbnd2ZWrsfLfcpuxHffMrJ1b7sCrAsNqlb6Y1eAMfwCVUNXt").unwrap();
        let share2 = base64::decode("hu+vT3+8/taHP7B/dWXh/g==").unwrap();
        let encrypted_share1 = base64::decode(
            "BEWObg41JiMJglSEA6Ebk37xOeflD2a1t2eiLmX0OPccJhAER5NmOI+4r4Cfm7aJn141sGKnTbCuIB9+AeVuw\
             MAQnzjsGPu5aNgkdpp+6VowAcVAV1DlzZvtwlQkCFlX4f3xmafTPFTPOokYi2a+H1n8GKwd",
        )
        .unwrap();
        let encrypted_share2 = base64::decode(
            "BNRzQ6TbqSc4pk0S8aziVRNjWm4DXQR5yCYTK2w22iSw4XAPW4OB9RxBpWVa1C/3ywVBT/3yLArOMXEsCEMOG\
             1+d2CiEvtuU1zADH2MVaCnXL/dVXkDchYZsvPWPkDcjQA==",
        )
        .unwrap();

        let priv_key1 = PrivateKey::from_base64(
            "BIl6j+J6dYttxALdjISDv6ZI4/VWVEhUzaS05LgrsfswmbLOg\
             Nt9HUC2E0w+9RqZx3XMkdEHBHfNuCSMpOwofVSq3TfyKwn0NrftKisKKVSaTOt5seJ67P5QL4hxgPWvxw==",
        )
        .unwrap();
        let priv_key2 = PrivateKey::from_base64(
            "BNNOqoU54GPo+1gTPv+hCgA9U2ZCKd76yOMrWa1xTWgeb4LhF\
             LMQIQoRwDVaW64g/WTdcxT4rDULoycUNFB60LER6hPEHg/ObBnRPV1rwS3nj9Bj0tbjVPPyL9p8QW8B+w==",
        )
        .unwrap();

        let decrypted1 = decrypt_share(&encrypted_share1, &priv_key1).unwrap();
        let decrypted2 = decrypt_share(&encrypted_share2, &priv_key2).unwrap();

        assert_eq!(decrypted1, share1);
        assert_eq!(decrypted2, share2);
    }
}
