/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    error::{JwCryptoError, Result},
    Algorithm, CompactJwe, DecryptionParameters, EncryptionAlgorithm, EncryptionParameters,
    JweHeader, Jwk, JwkKeyParameters,
};
use rc_crypto::{
    aead,
    agreement::{self, EphemeralKeyPair, InputKeyMaterial, UnparsedPublicKey},
    digest, rand,
};
use serde_derive::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct ECKeysParameters {
    pub crv: String,
    pub x: String,
    pub y: String,
}

pub(crate) fn encrypt_to_jwe(
    data: &[u8],
    encryption_params: EncryptionParameters,
) -> Result<CompactJwe> {
    let EncryptionParameters::ECDH_ES { enc, peer_jwk } = encryption_params;
    let local_key_pair = EphemeralKeyPair::generate(&agreement::ECDH_P256)?;
    let local_public_key = extract_pub_key_jwk(&local_key_pair)?;
    let JwkKeyParameters::EC(ref ec_key_params) = peer_jwk.key_parameters;
    let protected_header = JweHeader {
        kid: peer_jwk.kid.clone(),
        alg: Algorithm::ECDH_ES,
        enc,
        epk: Some(local_public_key),
        apu: None,
        apv: None,
    };

    let secret = derive_shared_secret(&protected_header, local_key_pair, &ec_key_params)?;

    let encryption_algorithm = match protected_header.enc {
        EncryptionAlgorithm::A256GCM => &aead::AES_256_GCM,
    };
    let sealing_key = aead::SealingKey::new(encryption_algorithm, &secret.as_ref())?;
    let additional_data = serde_json::to_string(&protected_header)?;
    let additional_data =
        base64::encode_config(additional_data.as_bytes(), base64::URL_SAFE_NO_PAD);
    let additional_data = additional_data.as_bytes();
    let aad = aead::Aad::from(additional_data);
    let mut iv: Vec<u8> = vec![0; 12];
    rand::fill(&mut iv)?;
    let nonce = aead::Nonce::try_assume_unique_for_key(encryption_algorithm, &iv)?;
    let mut encrypted = aead::seal(&sealing_key, nonce, aad, data)?;

    let tag_idx = encrypted.len() - encryption_algorithm.tag_len();
    let auth_tag = encrypted.split_off(tag_idx);
    let ciphertext = encrypted;

    Ok(CompactJwe::new(
        Some(protected_header),
        None,
        Some(iv),
        ciphertext,
        Some(auth_tag),
    )?)
}

pub(crate) fn decrypt_jwe(
    jwe: &CompactJwe,
    decryption_params: DecryptionParameters,
) -> Result<String> {
    let DecryptionParameters::ECDH_ES { local_key_pair } = decryption_params;

    let protected_header = jwe
        .protected_header()?
        .ok_or_else(|| JwCryptoError::IllegalState("protected_header must be present."))?;
    if protected_header.alg != Algorithm::ECDH_ES {
        return Err(JwCryptoError::IllegalState("alg mismatch."));
    }

    // Part 1: Reconstruct the secret.
    let peer_jwk = protected_header
        .epk
        .as_ref()
        .ok_or_else(|| JwCryptoError::IllegalState("epk not present"))?;
    let JwkKeyParameters::EC(ref ec_key_params) = peer_jwk.key_parameters;
    let secret = derive_shared_secret(&protected_header, local_key_pair, &ec_key_params)?;

    // Part 2: decrypt the payload
    if jwe.encrypted_key()?.is_some() {
        return Err(JwCryptoError::IllegalState(
            "The Encrypted Key must be empty.",
        ));
    }
    let encryption_algorithm = match protected_header.enc {
        EncryptionAlgorithm::A256GCM => &aead::AES_256_GCM,
    };
    let auth_tag = jwe
        .auth_tag()?
        .ok_or_else(|| JwCryptoError::IllegalState("auth_tag must be present."))?;
    if auth_tag.len() != encryption_algorithm.tag_len() {
        return Err(JwCryptoError::IllegalState(
            "The auth tag must be 16 bytes long.",
        ));
    }
    let iv = jwe
        .iv()?
        .ok_or_else(|| JwCryptoError::IllegalState("iv must be present."))?;
    let opening_key = aead::OpeningKey::new(&encryption_algorithm, &secret.as_ref())?;
    let ciphertext_and_tag: Vec<u8> = [jwe.ciphertext()?, auth_tag].concat();
    let nonce = aead::Nonce::try_assume_unique_for_key(&encryption_algorithm, &iv)?;
    let aad = aead::Aad::from(jwe.protected_header_raw().as_bytes());
    let plaintext = aead::open(&opening_key, nonce, aad, &ciphertext_and_tag)?;
    Ok(String::from_utf8(plaintext.to_vec())?)
}

fn derive_shared_secret(
    protected_header: &JweHeader,
    local_key_pair: EphemeralKeyPair,
    peer_key: &ECKeysParameters,
) -> Result<digest::Digest> {
    let (private_key, _) = local_key_pair.split();
    let peer_public_key_raw_bytes = public_key_from_ec_params(peer_key)?;
    let peer_public_key = UnparsedPublicKey::new(&agreement::ECDH_P256, &peer_public_key_raw_bytes);
    // Note: We don't support key-wrapping, but if we did `algorithm_id` would be `alg` instead.
    let algorithm_id = protected_header.enc.algorithm_id();
    let ikm = private_key.agree(&peer_public_key)?;
    let apu = protected_header.apu.as_deref().unwrap_or_default();
    let apv = protected_header.apv.as_deref().unwrap_or_default();
    get_secret_from_ikm(ikm, &apu, &apv, &algorithm_id)
}

fn public_key_from_ec_params(jwk: &ECKeysParameters) -> Result<Vec<u8>> {
    let x = base64::decode_config(&jwk.x, base64::URL_SAFE_NO_PAD)?;
    let y = base64::decode_config(&jwk.y, base64::URL_SAFE_NO_PAD)?;
    if jwk.crv != "P-256" {
        return Err(JwCryptoError::PartialImplementation(
            "Only P-256 curves are supported.",
        ));
    }
    if x.len() != (256 / 8) {
        return Err(JwCryptoError::IllegalState("X must be 32 bytes long."));
    }
    if y.len() != (256 / 8) {
        return Err(JwCryptoError::IllegalState("Y must be 32 bytes long."));
    }
    let mut peer_pub_key: Vec<u8> = vec![0x04];
    peer_pub_key.extend_from_slice(&x);
    peer_pub_key.extend_from_slice(&y);
    Ok(peer_pub_key)
}

fn get_secret_from_ikm(
    ikm: InputKeyMaterial,
    apu: &str,
    apv: &str,
    alg: &str,
) -> Result<digest::Digest> {
    let secret = ikm.derive(|z| {
        let mut buf: Vec<u8> = vec![];
        // ConcatKDF (1 iteration since keyLen <= hashLen).
        // See rfc7518 section 4.6 for reference.
        buf.extend_from_slice(&1u32.to_be_bytes());
        buf.extend_from_slice(&z);
        // otherinfo
        buf.extend_from_slice(&(alg.len() as u32).to_be_bytes());
        buf.extend_from_slice(alg.as_bytes());
        buf.extend_from_slice(&(apu.len() as u32).to_be_bytes());
        buf.extend_from_slice(apu.as_bytes());
        buf.extend_from_slice(&(apv.len() as u32).to_be_bytes());
        buf.extend_from_slice(apv.as_bytes());
        buf.extend_from_slice(&256u32.to_be_bytes());
        digest::digest(&digest::SHA256, &buf)
    })?;
    Ok(secret)
}

pub fn extract_pub_key_jwk(key_pair: &EphemeralKeyPair) -> Result<Jwk> {
    let pub_key_bytes = key_pair.public_key().to_bytes()?;
    // Uncompressed form (see SECG SEC1 section 2.3.3).
    // First byte is 4, then 32 bytes for x, and 32 bytes for y.
    assert_eq!(pub_key_bytes.len(), 1 + 32 + 32);
    assert_eq!(pub_key_bytes[0], 0x04);
    let x = Vec::from(&pub_key_bytes[1..33]);
    let x = base64::encode_config(&x, base64::URL_SAFE_NO_PAD);
    let y = Vec::from(&pub_key_bytes[33..]);
    let y = base64::encode_config(&y, base64::URL_SAFE_NO_PAD);
    Ok(Jwk {
        kid: None,
        key_parameters: JwkKeyParameters::EC(ECKeysParameters {
            crv: "P-256".to_owned(),
            x,
            y,
        }),
    })
}
