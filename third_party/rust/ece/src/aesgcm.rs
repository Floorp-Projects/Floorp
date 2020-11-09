/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This supports the now obsolete HTTP-ECE Draft 02 "aesgcm" content
 * type. There are a number of providers that still use this format,
 * and there's no real mechanism to return the client supported crypto
 * versions.
 *
 * */

use crate::{
    common::*,
    crypto::{self, LocalKeyPair, RemotePublicKey},
    error::*,
};
use std::collections::HashMap;

const ECE_AESGCM_PAD_SIZE: usize = 2;

const ECE_WEBPUSH_AESGCM_KEYPAIR_LENGTH: usize = 134; // (2 + Raw Key Length) * 2
const ECE_WEBPUSH_AESGCM_AUTHINFO: &str = "Content-Encoding: auth\0";

// a DER prefixed key is "\04" + ECE_WEBPUSH_RAW_KEY_LENGTH
const ECE_WEBPUSH_RAW_KEY_LENGTH: usize = 65;
const ECE_WEBPUSH_IKM_LENGTH: usize = 32;

pub struct AesGcmEncryptedBlock {
    pub dh: Vec<u8>,
    pub salt: Vec<u8>,
    pub rs: u32,
    pub ciphertext: Vec<u8>,
}

impl AesGcmEncryptedBlock {
    pub fn aesgcm_rs(rs: u32) -> u32 {
        if rs > u32::max_value() - ECE_TAG_LENGTH as u32 {
            return 0;
        }
        rs + ECE_TAG_LENGTH as u32
    }

    /// Create a new block from the various header strings and body content.
    pub fn new(
        dh: &[u8],
        salt: &[u8],
        rs: u32,
        ciphertext: Vec<u8>,
    ) -> Result<AesGcmEncryptedBlock> {
        Ok(AesGcmEncryptedBlock {
            dh: dh.to_owned(),
            salt: salt.to_owned(),
            rs: Self::aesgcm_rs(rs),
            ciphertext,
        })
    }

    /// Return the headers Hash, NOTE you may need to merge Crypto-Key if there's
    /// already a VAPID element present.
    pub fn headers(self) -> HashMap<String, String> {
        let mut result: HashMap<String, String> = HashMap::new();
        let mut rs = "".to_owned();
        result.insert(
            "Crypto-Key".to_owned(),
            format!(
                "dh={}",
                base64::encode_config(&self.dh, base64::URL_SAFE_NO_PAD)
            ),
        );
        if self.rs > 0 {
            rs = format!(";rs={}", self.rs);
        }
        result.insert(
            "Encryption".to_owned(),
            format!(
                "salt={}{}",
                base64::encode_config(&self.salt, base64::URL_SAFE_NO_PAD),
                rs
            ),
        );
        result
    }

    /// Encode the body as a String.
    /// If you need the bytes, probably just call .ciphertext directly
    pub fn body(self) -> String {
        base64::encode_config(&self.ciphertext, base64::URL_SAFE_NO_PAD)
    }
}

pub struct AesGcmEceWebPush;
impl AesGcmEceWebPush {
    /// Encrypts a Web Push message using the "aesgcm" scheme. This function
    /// automatically generates an ephemeral ECDH key pair.
    pub fn encrypt(
        remote_pub_key: &dyn RemotePublicKey,
        auth_secret: &[u8],
        plaintext: &[u8],
        params: WebPushParams,
    ) -> Result<AesGcmEncryptedBlock> {
        let cryptographer = crypto::holder::get_cryptographer();
        let local_prv_key = cryptographer.generate_ephemeral_keypair()?;
        Self::encrypt_with_keys(
            &*local_prv_key,
            remote_pub_key,
            auth_secret,
            plaintext,
            params,
        )
    }

    /// Encrypts a Web Push message using the "aesgcm" scheme, with an explicit
    /// sender key. The sender key can be reused.
    pub fn encrypt_with_keys(
        local_prv_key: &dyn LocalKeyPair,
        remote_pub_key: &dyn RemotePublicKey,
        auth_secret: &[u8],
        plaintext: &[u8],
        params: WebPushParams,
    ) -> Result<AesGcmEncryptedBlock> {
        let cryptographer = crypto::holder::get_cryptographer();
        let salt = {
            let mut salt = [0u8; ECE_SALT_LENGTH];
            cryptographer.random_bytes(&mut salt)?;
            salt.to_vec()
        };
        let raw_local_pub_key = local_prv_key.pub_as_raw()?;
        let ciphertext = Self::common_encrypt(
            local_prv_key,
            remote_pub_key,
            auth_secret,
            &salt,
            params.rs,
            params.pad_length,
            plaintext,
        )?;
        Ok(AesGcmEncryptedBlock {
            salt,
            dh: raw_local_pub_key,
            rs: params.rs,
            ciphertext,
        })
    }

    /// Decrypts a Web Push message encrypted using the "aesgcm" scheme.
    pub fn decrypt(
        local_prv_key: &dyn LocalKeyPair,
        auth_secret: &[u8],
        block: &AesGcmEncryptedBlock,
    ) -> Result<Vec<u8>> {
        let cryptographer = crypto::holder::get_cryptographer();
        let sender_key = cryptographer.import_public_key(&block.dh)?;
        Self::common_decrypt(
            local_prv_key,
            &*sender_key,
            auth_secret,
            &block.salt,
            block.rs,
            &block.ciphertext,
        )
    }
}

impl EceWebPush for AesGcmEceWebPush {
    fn needs_trailer(rs: u32, ciphertextlen: usize) -> bool {
        ciphertextlen as u32 % rs == 0
    }

    fn pad_size() -> usize {
        ECE_AESGCM_PAD_SIZE
    }

    fn min_block_pad_length(pad_len: usize, max_block_len: usize) -> usize {
        ece_min_block_pad_length(pad_len, max_block_len)
    }

    fn pad(plaintext: &[u8], _: usize, _: bool) -> Result<Vec<u8>> {
        let plen = plaintext.len();
        let mut block = vec![0; plen + ECE_AESGCM_PAD_SIZE];
        block[2..].copy_from_slice(plaintext);
        Ok(block)
    }

    fn unpad(block: &[u8], _: bool) -> Result<&[u8]> {
        let padding_size = (((block[0] as u16) << 8) | block[1] as u16) as usize;
        if padding_size >= block.len() - 2 {
            return Err(Error::DecryptPadding);
        }
        if block[2..(2 + padding_size)].iter().any(|b| *b != 0u8) {
            return Err(Error::DecryptPadding);
        }
        Ok(&block[(2 + padding_size)..])
    }

    /// Derives the "aesgcm" decryption keyn and nonce given the receiver private
    /// key, sender public key, authentication secret, and sender salt.
    fn derive_key_and_nonce(
        ece_mode: EceMode,
        local_prv_key: &dyn LocalKeyPair,
        remote_pub_key: &dyn RemotePublicKey,
        auth_secret: &[u8],
        salt: &[u8],
    ) -> Result<KeyAndNonce> {
        let cryptographer = crypto::holder::get_cryptographer();
        let shared_secret = cryptographer.compute_ecdh_secret(remote_pub_key, local_prv_key)?;
        let raw_remote_pub_key = remote_pub_key.as_raw()?;
        let raw_local_pub_key = local_prv_key.pub_as_raw()?;

        let keypair = match ece_mode {
            EceMode::ENCRYPT => encode_keys(&raw_remote_pub_key, &raw_local_pub_key),
            EceMode::DECRYPT => encode_keys(&raw_local_pub_key, &raw_remote_pub_key),
        }?;
        let keyinfo = generate_info("aesgcm", &keypair)?;
        let nonceinfo = generate_info("nonce", &keypair)?;
        let ikm = cryptographer.hkdf_sha256(
            auth_secret,
            &shared_secret,
            &ECE_WEBPUSH_AESGCM_AUTHINFO.as_bytes(),
            ECE_WEBPUSH_IKM_LENGTH,
        )?;
        let key = cryptographer.hkdf_sha256(salt, &ikm, &keyinfo, ECE_AES_KEY_LENGTH)?;
        let nonce = cryptographer.hkdf_sha256(salt, &ikm, &nonceinfo, ECE_NONCE_LENGTH)?;
        Ok((key, nonce))
    }
}

fn encode_keys(raw_key1: &[u8], raw_key2: &[u8]) -> Result<Vec<u8>> {
    let mut combined = vec![0u8; ECE_WEBPUSH_AESGCM_KEYPAIR_LENGTH];

    if raw_key1.len() > ECE_WEBPUSH_RAW_KEY_LENGTH || raw_key2.len() > ECE_WEBPUSH_RAW_KEY_LENGTH {
        return Err(Error::InvalidKeyLength);
    }
    // length prefix each key
    combined[0] = 0;
    combined[1] = 65;
    combined[2..67].copy_from_slice(raw_key1);
    combined[67] = 0;
    combined[68] = 65;
    combined[69..].copy_from_slice(raw_key2);
    Ok(combined)
}

// The "aesgcm" IKM info string is "WebPush: info", followed by the
// receiver and sender public keys prefixed by their lengths.
fn generate_info(encoding: &str, keypair: &[u8]) -> Result<Vec<u8>> {
    let info_str = format!("Content-Encoding: {}\0P-256\0", encoding);
    let offset = info_str.len();
    let mut info = vec![0u8; offset + keypair.len()];
    info[0..offset].copy_from_slice(info_str.as_bytes());
    info[offset..offset + ECE_WEBPUSH_AESGCM_KEYPAIR_LENGTH].copy_from_slice(keypair);
    Ok(info)
}
