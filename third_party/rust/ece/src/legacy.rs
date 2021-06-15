/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    aesgcm::{AesGcmEceWebPush, AesGcmEncryptedBlock},
    common::WebPushParams,
    crypto::EcKeyComponents,
    error::*,
};

/// Encrypt a block using legacy AESGCM encoding.
///
/// * `remote_pub` : the remote public key
/// * `remote_auth` : the remote authorization token
/// * `salt` : the locally generated random salt
/// * `data` : the data to encrypt
///
pub fn encrypt_aesgcm(
    remote_pub: &[u8],
    remote_auth: &[u8],
    salt: &[u8],
    data: &[u8],
) -> Result<AesGcmEncryptedBlock> {
    let cryptographer = crate::crypto::holder::get_cryptographer();
    let remote_key = cryptographer.import_public_key(remote_pub)?;
    let local_key_pair = cryptographer.generate_ephemeral_keypair()?;
    let mut padr = [0u8; 2];
    cryptographer.random_bytes(&mut padr)?;
    // since it's a sampled random, endian doesn't really matter.
    let pad = ((usize::from(padr[0]) + (usize::from(padr[1]) << 8)) % 4095) + 1;
    let params = WebPushParams::new(4096, pad, Vec::from(salt));
    AesGcmEceWebPush::encrypt_with_keys(&*local_key_pair, &*remote_key, &remote_auth, data, params)
}

/// Decrypt a block using legacy AESGCM encoding.
///
/// * `components` : the locally generated private key components.
/// * `auth` : the locally generated auth token (this value was shared with the encryptor).
/// * `data` : the encrypted data block
///
pub fn decrypt_aesgcm(
    components: &EcKeyComponents,
    auth: &[u8],
    data: &AesGcmEncryptedBlock,
) -> Result<Vec<u8>> {
    let cryptographer = crate::crypto::holder::get_cryptographer();
    let priv_key = cryptographer.import_key_pair(components).unwrap();
    AesGcmEceWebPush::decrypt(&*priv_key, &auth, data)
}

#[cfg(all(test, feature = "backend-openssl"))]
mod aesgcm_tests {
    use super::*;
    use hex;

    #[derive(Debug)]
    struct AesGcmTestPayload {
        dh: String,
        salt: String,
        rs: u32,
        ciphertext: String,
    }

    #[allow(clippy::too_many_arguments)]
    fn try_encrypt(
        private_key: &str,
        public_key: &str,
        remote_pub_key: &str,
        auth_secret: &str,
        salt: &str,
        pad_length: usize,
        rs: u32,
        plaintext: &str,
    ) -> Result<AesGcmTestPayload> {
        let cryptographer = crate::crypto::holder::get_cryptographer();
        let private_key = hex::decode(private_key).unwrap();
        let public_key = hex::decode(public_key).unwrap();
        let ec_key = EcKeyComponents::new(private_key, public_key);
        let local_key_pair = cryptographer.import_key_pair(&ec_key)?;
        let remote_pub_key = hex::decode(remote_pub_key).unwrap();
        let remote_pub_key = cryptographer.import_public_key(&remote_pub_key).unwrap();
        let auth_secret = hex::decode(auth_secret).unwrap();
        let salt = hex::decode(salt).unwrap();
        let plaintext = plaintext.as_bytes();
        let params = WebPushParams::new(rs, pad_length, salt);
        let encrypted_block = AesGcmEceWebPush::encrypt_with_keys(
            &*local_key_pair,
            &*remote_pub_key,
            &auth_secret,
            &plaintext,
            params,
        )?;
        Ok(AesGcmTestPayload {
            dh: hex::encode(encrypted_block.dh),
            salt: hex::encode(encrypted_block.salt),
            rs: encrypted_block.rs,
            ciphertext: hex::encode(encrypted_block.ciphertext),
        })
    }

    fn try_decrypt(
        private_key: &str,
        public_key: &str,
        auth_secret: &str,
        payload: &AesGcmTestPayload,
    ) -> Result<String> {
        let private_key = hex::decode(private_key).unwrap();
        let public_key = hex::decode(public_key).unwrap();
        let ec_key = EcKeyComponents::new(private_key, public_key);
        let plaintext = decrypt_aesgcm(
            &ec_key,
            &hex::decode(auth_secret).unwrap(),
            &AesGcmEncryptedBlock::new(
                &hex::decode(&payload.dh).unwrap(),
                &hex::decode(&payload.salt).unwrap(),
                payload.rs,
                hex::decode(&payload.ciphertext).unwrap(),
            )?,
        )?;
        Ok(String::from_utf8(plaintext).unwrap())
    }

    #[test]
    fn test_e2e() {
        let (local_key, remote_key) = crate::generate_keys().unwrap();
        let plaintext = b"There was a green mouse, running in the grass";
        let mut auth_secret = vec![0u8; 16];
        let cryptographer = crate::crypto::holder::get_cryptographer();
        cryptographer.random_bytes(&mut auth_secret).unwrap();
        let remote_public = cryptographer
            .import_public_key(&remote_key.pub_as_raw().unwrap())
            .unwrap();
        let params = WebPushParams::default();
        let encrypted_block = AesGcmEceWebPush::encrypt_with_keys(
            &*local_key,
            &*remote_public,
            &auth_secret,
            plaintext,
            params,
        )
        .unwrap();
        let decrypted =
            AesGcmEceWebPush::decrypt(&*remote_key, &auth_secret, &encrypted_block).unwrap();
        assert_eq!(decrypted, plaintext.to_vec());
    }

    #[test]
    fn test_conv_fn() -> Result<()> {
        let (local_key, auth) = crate::generate_keypair_and_auth_secret()?;
        let plaintext = b"There was a little ship that had never sailed";
        let mut salt = vec![0u8; 16];
        let cryptographer = crate::crypto::holder::get_cryptographer();
        cryptographer.random_bytes(&mut salt)?;
        let encoded = encrypt_aesgcm(&local_key.pub_as_raw()?, &auth, &salt, plaintext).unwrap();
        let decoded = decrypt_aesgcm(&local_key.raw_components()?, &auth, &encoded)?;
        assert_eq!(decoded, plaintext.to_vec());
        Ok(())
    }

    #[test]
    fn try_encrypt_ietf_rfc() {
        // Test data from [IETF Web Push Encryption Draft 5](https://tools.ietf.org/html/draft-ietf-webpush-encryption-04#section-5)
        let encrypted_block = try_encrypt(
            "9c249c7a4f90a448e638e953fab437f27673bdd3e5a9ad34672d22ea6d8e26f6",
            "04da110db6fce091a6f20e59e42171bab4aab17589d7522d7d71166152c4f3963b0989038d7b0811ce1aab161a4351bc06a917089e833e90eb5ad7568ff9ae8075",
            "042124063ccbf19dc2fa88b643ba04e6dd8da7ea7ba2c8c62e0f77a943f4c2fa914f6d44116c9fd1c40341c6a440cab3e2140a60e4378a5da735972de078005105",
            "476f6f20676f6f206727206a6f6f6221",
            "96781aadbc8a7cca22f59ef9c585e692",
            0,
            4096,
            "I am the walrus",
        ).unwrap();
        assert_eq!(
            encrypted_block.ciphertext,
            "ea7a80414304f2136ac39277925f1ca55549ca55ca62a64e7ac7991bc52e78aa40"
        );
    }

    #[test]
    fn test_decrypt_ietf_rfc() {
        // Test data from [IETF Web Push Encryption Draft 5](https://tools.ietf.org/html/draft-ietf-webpush-encryption-04#section-5)
        let plaintext = try_decrypt(
            "f455a5d79fd05100160da0f7937979d19059409e1abb6ec5d55e05d2e2d20ff3",
            "042124063ccbf19dc2fa88b643ba04e6dd8da7ea7ba2c8c62e0f77a943f4c2fa914f6d44116c9fd1c40341c6a440cab3e2140a60e4378a5da735972de078005105",
            "476f6f20676f6f206727206a6f6f6221",
            &AesGcmTestPayload {
                ciphertext : "ea7a80414304f2136ac39277925f1ca55549ca55ca62a64e7ac7991bc52e78aa40".to_owned(),
                salt : "96781aadbc8a7cca22f59ef9c585e692".to_owned(),
                dh : "04da110db6fce091a6f20e59e42171bab4aab17589d7522d7d71166152c4f3963b0989038d7b0811ce1aab161a4351bc06a917089e833e90eb5ad7568ff9ae8075".to_owned(),
                rs : 4096,
            }
        ).unwrap();
        assert_eq!(plaintext, "I am the walrus");
    }
}
