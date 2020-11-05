/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Theorically, everything done in this crate could and should be done in a JWT library.
//! However, none of the existing rust JWT libraries can handle ECDH-ES encryption, and API choices
//! made by their authors make it difficult to add this feature.
//! In the past, we chose cjose to do that job, but it added three C dependencies to build and link
//! against: jansson, openssl and cjose itself.

pub use error::JwCryptoError;
use error::Result;
use rc_crypto::agreement::EphemeralKeyPair;
use serde_derive::{Deserialize, Serialize};
use std::str::FromStr;

pub mod ec;
mod error;

pub enum EncryptionParameters<'a> {
    // ECDH-ES in Direct Key Agreement mode.
    #[allow(non_camel_case_types)]
    ECDH_ES {
        enc: EncryptionAlgorithm,
        peer_jwk: &'a Jwk,
    },
}

pub enum DecryptionParameters {
    // ECDH-ES in Direct Key Agreement mode.
    #[allow(non_camel_case_types)]
    ECDH_ES { local_key_pair: EphemeralKeyPair },
}

#[derive(Serialize, Deserialize, Debug, PartialEq)]
enum Algorithm {
    #[serde(rename = "ECDH-ES")]
    #[allow(non_camel_case_types)]
    ECDH_ES,
}
#[derive(Serialize, Deserialize, Debug)]
pub enum EncryptionAlgorithm {
    A256GCM,
}

impl EncryptionAlgorithm {
    fn algorithm_id(&self) -> &'static str {
        match self {
            Self::A256GCM => "A256GCM",
        }
    }
}

#[derive(Serialize, Deserialize, Debug)]
struct JweHeader {
    alg: Algorithm,
    enc: EncryptionAlgorithm,
    #[serde(skip_serializing_if = "Option::is_none")]
    kid: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    epk: Option<Jwk>,
    #[serde(skip_serializing_if = "Option::is_none")]
    apu: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    apv: Option<String>,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Jwk {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub kid: Option<String>,
    #[serde(flatten)]
    pub key_parameters: JwkKeyParameters,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
#[serde(tag = "kty")]
pub enum JwkKeyParameters {
    EC(ec::ECKeysParameters),
}

#[derive(Debug)]
pub struct CompactJwe {
    jwe_segments: Vec<String>,
}

impl CompactJwe {
    // A builder pattern would be nicer, but this will do for now.
    fn new(
        protected_header: Option<JweHeader>,
        encrypted_key: Option<Vec<u8>>,
        iv: Option<Vec<u8>>,
        ciphertext: Vec<u8>,
        auth_tag: Option<Vec<u8>>,
    ) -> Result<Self> {
        let protected_header = protected_header
            .as_ref()
            .map(|h| serde_json::to_string(&h))
            .transpose()?
            .map(|h| base64::encode_config(&h, base64::URL_SAFE_NO_PAD))
            .unwrap_or_default();
        let encrypted_key = encrypted_key
            .as_ref()
            .map(|k| base64::encode_config(&k, base64::URL_SAFE_NO_PAD))
            .unwrap_or_default();
        let iv = iv
            .as_ref()
            .map(|iv| base64::encode_config(&iv, base64::URL_SAFE_NO_PAD))
            .unwrap_or_default();
        let ciphertext = base64::encode_config(&ciphertext, base64::URL_SAFE_NO_PAD);
        let auth_tag = auth_tag
            .as_ref()
            .map(|t| base64::encode_config(&t, base64::URL_SAFE_NO_PAD))
            .unwrap_or_default();
        let jwe_segments = vec![protected_header, encrypted_key, iv, ciphertext, auth_tag];
        Ok(Self { jwe_segments })
    }

    fn protected_header(&self) -> Result<Option<JweHeader>> {
        Ok(self
            .try_deserialize_base64_segment(0)?
            .map(|s| serde_json::from_slice(&s))
            .transpose()?)
    }

    fn protected_header_raw(&self) -> &str {
        &self.jwe_segments[0]
    }

    fn encrypted_key(&self) -> Result<Option<Vec<u8>>> {
        self.try_deserialize_base64_segment(1)
    }

    fn iv(&self) -> Result<Option<Vec<u8>>> {
        self.try_deserialize_base64_segment(2)
    }

    fn ciphertext(&self) -> Result<Vec<u8>> {
        Ok(self
            .try_deserialize_base64_segment(3)?
            .ok_or_else(|| JwCryptoError::IllegalState("Ciphertext is empty"))?)
    }

    fn auth_tag(&self) -> Result<Option<Vec<u8>>> {
        self.try_deserialize_base64_segment(4)
    }

    fn try_deserialize_base64_segment(&self, index: usize) -> Result<Option<Vec<u8>>> {
        Ok(match self.jwe_segments[index].is_empty() {
            true => None,
            false => Some(base64::decode_config(
                &self.jwe_segments[index],
                base64::URL_SAFE_NO_PAD,
            )?),
        })
    }
}

impl FromStr for CompactJwe {
    type Err = JwCryptoError;
    fn from_str(str: &str) -> Result<Self> {
        let jwe_segments: Vec<String> = str.split('.').map(|s| s.to_owned()).collect();
        if jwe_segments.len() != 5 {
            return Err(JwCryptoError::DeserializationError);
        }
        Ok(Self { jwe_segments })
    }
}

impl ToString for CompactJwe {
    fn to_string(&self) -> String {
        assert!(self.jwe_segments.len() == 5);
        self.jwe_segments.join(".")
    }
}

/// Encrypt and serialize data in the JWE compact form.
pub fn encrypt_to_jwe(data: &[u8], encryption_params: EncryptionParameters) -> Result<String> {
    let jwe = match encryption_params {
        EncryptionParameters::ECDH_ES { .. } => ec::encrypt_to_jwe(data, encryption_params)?,
    };
    Ok(jwe.to_string())
}

/// Deserialize and decrypt data in the JWE compact form.
pub fn decrypt_jwe(jwe: &str, decryption_params: DecryptionParameters) -> Result<String> {
    let jwe = jwe.parse()?;
    Ok(match decryption_params {
        DecryptionParameters::ECDH_ES { .. } => ec::decrypt_jwe(&jwe, decryption_params)?,
    })
}

#[test]
fn test_encrypt_decrypt_jwe_ecdh_es() {
    use rc_crypto::agreement;
    let key_pair = EphemeralKeyPair::generate(&agreement::ECDH_P256).unwrap();
    let jwk = ec::extract_pub_key_jwk(&key_pair).unwrap();
    let data = b"The big brown fox jumped over... What?";
    let encrypted = encrypt_to_jwe(
        data,
        EncryptionParameters::ECDH_ES {
            enc: EncryptionAlgorithm::A256GCM,
            peer_jwk: &jwk,
        },
    )
    .unwrap();
    let decrypted = decrypt_jwe(
        &encrypted,
        DecryptionParameters::ECDH_ES {
            local_key_pair: key_pair,
        },
    )
    .unwrap();
    assert_eq!(decrypted, std::str::from_utf8(data).unwrap());
}

#[test]
fn test_compact_jwe_roundtrip() {
    let mut iv = [0u8; 16];
    rc_crypto::rand::fill(&mut iv).unwrap();
    let mut ciphertext = [0u8; 243];
    rc_crypto::rand::fill(&mut ciphertext).unwrap();
    let mut auth_tag = [0u8; 16];
    rc_crypto::rand::fill(&mut auth_tag).unwrap();
    let jwe = CompactJwe::new(
        Some(JweHeader {
            alg: Algorithm::ECDH_ES,
            enc: EncryptionAlgorithm::A256GCM,
            kid: None,
            epk: None,
            apu: None,
            apv: None,
        }),
        None,
        Some(iv.to_vec()),
        ciphertext.to_vec(),
        Some(auth_tag.to_vec()),
    )
    .unwrap();
    let compacted = jwe.to_string();
    let jwe2: CompactJwe = compacted.parse().unwrap();
    assert_eq!(jwe.jwe_segments, jwe2.jwe_segments);
}
