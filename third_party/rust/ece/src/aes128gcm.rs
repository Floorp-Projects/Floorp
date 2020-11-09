/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    common::*,
    crypto::{self, LocalKeyPair, RemotePublicKey},
    error::*,
};
use byteorder::{BigEndian, ByteOrder};

const ECE_AES128GCM_MIN_RS: u32 = 18;
const ECE_AES128GCM_HEADER_LENGTH: usize = 21;
// The max AES128GCM Key ID Length is 255 octets. We use far less of that because we use
// the "key_id" to store the exchanged public key since we don't cache the key_ids.
// Code fails if the key_id is not a public key length field.
const ECE_AES128GCM_PAD_SIZE: usize = 1;

const ECE_WEBPUSH_AES128GCM_IKM_INFO_PREFIX: &str = "WebPush: info\0";
const ECE_WEBPUSH_AES128GCM_IKM_INFO_LENGTH: usize = 144; // 14 (prefix len) + 65 (pub key len) * 2;

const ECE_WEBPUSH_IKM_LENGTH: usize = 32;
const ECE_AES128GCM_KEY_INFO: &str = "Content-Encoding: aes128gcm\0";
const ECE_AES128GCM_NONCE_INFO: &str = "Content-Encoding: nonce\0";

// TODO: When done, remove the aes128gcm prefixes and the EC_ ones.
// As for now it makes it easier to Ctrl + F into ecec :)

pub struct Aes128GcmEceWebPush;
impl Aes128GcmEceWebPush {
    /// Encrypts a Web Push message using the "aes128gcm" scheme. This function
    /// automatically generates an ephemeral ECDH key pair.
    pub fn encrypt(
        remote_pub_key: &dyn RemotePublicKey,
        auth_secret: &[u8],
        plaintext: &[u8],
        params: WebPushParams,
    ) -> Result<Vec<u8>> {
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

    /// Encrypts a Web Push message using the "aes128gcm" scheme, with an explicit
    /// sender key. The sender key can be reused.
    pub fn encrypt_with_keys(
        local_prv_key: &dyn LocalKeyPair,
        remote_pub_key: &dyn RemotePublicKey,
        auth_secret: &[u8],
        plaintext: &[u8],
        params: WebPushParams,
    ) -> Result<Vec<u8>> {
        let cryptographer = crypto::holder::get_cryptographer();
        let salt = match params.salt {
            Some(salt) => salt,
            None => {
                let mut salt = [0u8; ECE_SALT_LENGTH];
                cryptographer.random_bytes(&mut salt)?;
                salt.to_vec()
            }
        };
        let mut header = vec![0u8; ECE_AES128GCM_HEADER_LENGTH + ECE_WEBPUSH_PUBLIC_KEY_LENGTH];
        header[0..ECE_SALT_LENGTH].copy_from_slice(&salt);
        BigEndian::write_u32(&mut header[ECE_SALT_LENGTH..], params.rs);
        header[ECE_SALT_LENGTH + 4] = ECE_WEBPUSH_PUBLIC_KEY_LENGTH as u8;
        let raw_local_pub_key = local_prv_key.pub_as_raw()?;
        header[ECE_AES128GCM_HEADER_LENGTH
            ..ECE_AES128GCM_HEADER_LENGTH + ECE_WEBPUSH_PUBLIC_KEY_LENGTH]
            .copy_from_slice(&raw_local_pub_key);
        let mut ciphertext = Self::common_encrypt(
            local_prv_key,
            remote_pub_key,
            auth_secret,
            &salt,
            params.rs,
            params.pad_length,
            plaintext,
        )?;
        // TODO: Not efficient and probably allocates more,
        // we should allocate the buffer upfront if possible.
        header.append(&mut ciphertext);
        Ok(header)
    }

    /// Decrypts a Web Push message encrypted using the "aes128gcm" scheme.
    pub fn decrypt(
        local_prv_key: &dyn LocalKeyPair,
        auth_secret: &[u8],
        payload: &[u8],
    ) -> Result<Vec<u8>> {
        if payload.len() < ECE_AES128GCM_HEADER_LENGTH {
            return Err(Error::HeaderTooShort);
        }

        let key_id_len = payload[ECE_SALT_LENGTH + 4] as usize;
        if payload.len() < ECE_AES128GCM_HEADER_LENGTH + key_id_len {
            return Err(Error::HeaderTooShort);
        }

        let rs = BigEndian::read_u32(&payload[ECE_SALT_LENGTH..]);
        if rs < ECE_AES128GCM_MIN_RS {
            return Err(Error::InvalidRecordSize);
        }

        let salt = &payload[0..ECE_SALT_LENGTH];
        if key_id_len != ECE_WEBPUSH_PUBLIC_KEY_LENGTH {
            return Err(Error::InvalidKeyLength);
        }
        let key_id_pos = ECE_AES128GCM_HEADER_LENGTH;
        let key_id = &payload[key_id_pos..key_id_pos + key_id_len];

        let ciphertext_start = ECE_AES128GCM_HEADER_LENGTH + key_id_len;
        if payload.len() == ciphertext_start {
            return Err(Error::ZeroCiphertext);
        }
        let ciphertext = &payload[ciphertext_start..];
        let cryptographer = crypto::holder::get_cryptographer();
        let key = cryptographer.import_public_key(key_id)?;
        Self::common_decrypt(local_prv_key, &*key, auth_secret, salt, rs, ciphertext)
    }
}

impl EceWebPush for Aes128GcmEceWebPush {
    /// Always returns false because "aes128gcm" uses
    /// a padding scheme that doesn't need a trailer.
    fn needs_trailer(_: u32, _: usize) -> bool {
        false
    }

    fn pad_size() -> usize {
        ECE_AES128GCM_PAD_SIZE
    }

    fn min_block_pad_length(pad_len: usize, max_block_len: usize) -> usize {
        ece_min_block_pad_length(pad_len, max_block_len)
    }

    fn pad(plaintext: &[u8], block_pad_len: usize, last_record: bool) -> Result<Vec<u8>> {
        let mut block = Vec::with_capacity(plaintext.len() + 1 /* delimiter */ + block_pad_len);
        block.extend_from_slice(plaintext);
        block.push(if last_record { 2 } else { 1 });
        let padding = vec![0u8; block_pad_len];
        block.extend(padding);
        Ok(block)
    }

    fn unpad(block: &[u8], last_record: bool) -> Result<&[u8]> {
        let pos = match block.iter().rposition(|&b| b != 0) {
            Some(pos) => pos,
            None => return Err(Error::ZeroCiphertext),
        };
        let expected_delim = if last_record { 2 } else { 1 };
        if block[pos] != expected_delim {
            return Err(Error::DecryptPadding);
        }
        Ok(&block[..pos])
    }

    /// Derives the "aes128gcm" decryption key and nonce given the receiver private
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

        // The new "aes128gcm" scheme includes the sender and receiver public keys in
        // the info string when deriving the Web Push IKM.
        let ikm_info = match ece_mode {
            EceMode::ENCRYPT => generate_info(&raw_remote_pub_key, &raw_local_pub_key),
            EceMode::DECRYPT => generate_info(&raw_local_pub_key, &raw_remote_pub_key),
        }?;
        let cryptographer = crypto::holder::get_cryptographer();
        let ikm = cryptographer.hkdf_sha256(
            auth_secret,
            &shared_secret,
            &ikm_info,
            ECE_WEBPUSH_IKM_LENGTH,
        )?;
        let key = cryptographer.hkdf_sha256(
            salt,
            &ikm,
            ECE_AES128GCM_KEY_INFO.as_bytes(),
            ECE_AES_KEY_LENGTH,
        )?;
        let nonce = cryptographer.hkdf_sha256(
            salt,
            &ikm,
            ECE_AES128GCM_NONCE_INFO.as_bytes(),
            ECE_NONCE_LENGTH,
        )?;
        Ok((key, nonce))
    }
}

// The "aes128gcm" IKM info string is "WebPush: info\0", followed by the
// receiver and sender public keys.
fn generate_info(
    raw_recv_pub_key: &[u8],
    raw_sender_pub_key: &[u8],
) -> Result<[u8; ECE_WEBPUSH_AES128GCM_IKM_INFO_LENGTH]> {
    let mut info = [0u8; ECE_WEBPUSH_AES128GCM_IKM_INFO_LENGTH];
    let prefix = ECE_WEBPUSH_AES128GCM_IKM_INFO_PREFIX.as_bytes();
    let mut offset = prefix.len();
    info[0..offset].copy_from_slice(prefix);
    info[offset..offset + ECE_WEBPUSH_PUBLIC_KEY_LENGTH].copy_from_slice(raw_recv_pub_key);
    offset += ECE_WEBPUSH_PUBLIC_KEY_LENGTH;
    info[offset..].copy_from_slice(raw_sender_pub_key);
    Ok(info)
}
