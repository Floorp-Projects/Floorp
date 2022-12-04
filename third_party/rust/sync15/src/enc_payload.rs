/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error;
use crate::key_bundle::KeyBundle;
use lazy_static::lazy_static;
use serde::{Deserialize, Serialize};

/// A representation of an encrypted payload. Used as the payload in EncryptedBso and
/// also anywhere else the sync keys might be used to encrypt/decrypt, such as send-tab payloads.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct EncryptedPayload {
    #[serde(rename = "IV")]
    pub iv: String,
    pub hmac: String,
    pub ciphertext: String,
}

impl EncryptedPayload {
    #[inline]
    pub fn serialized_len(&self) -> usize {
        (*EMPTY_ENCRYPTED_PAYLOAD_SIZE) + self.ciphertext.len() + self.hmac.len() + self.iv.len()
    }

    pub fn decrypt(&self, key: &KeyBundle) -> error::Result<String> {
        key.decrypt(&self.ciphertext, &self.iv, &self.hmac)
    }

    pub fn decrypt_into<T>(&self, key: &KeyBundle) -> error::Result<T>
    where
        for<'a> T: Deserialize<'a>,
    {
        Ok(serde_json::from_str(&self.decrypt(key)?)?)
    }

    pub fn from_cleartext(key: &KeyBundle, cleartext: String) -> error::Result<Self> {
        let (enc_base64, iv_base64, hmac_base16) =
            key.encrypt_bytes_rand_iv(cleartext.as_bytes())?;
        Ok(EncryptedPayload {
            iv: iv_base64,
            hmac: hmac_base16,
            ciphertext: enc_base64,
        })
    }

    pub fn from_cleartext_payload<T: Serialize>(
        key: &KeyBundle,
        cleartext_payload: &T,
    ) -> error::Result<Self> {
        Self::from_cleartext(key, serde_json::to_string(cleartext_payload)?)
    }
}

// Our "postqueue", which chunks records for upload, needs to know this value.
// It's tricky to determine at compile time, so do it once at at runtime.
lazy_static! {
    // The number of bytes taken up by padding in a EncryptedPayload.
    static ref EMPTY_ENCRYPTED_PAYLOAD_SIZE: usize = serde_json::to_string(
        &EncryptedPayload { iv: "".into(), hmac: "".into(), ciphertext: "".into() }
    ).unwrap().len();
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[derive(Serialize, Deserialize, Debug)]
    struct TestStruct {
        id: String,
        age: u32,
        meta: String,
    }

    #[test]
    fn test_roundtrip_crypt_record() {
        let key = KeyBundle::new_random().unwrap();
        let payload_json = json!({ "id": "aaaaaaaaaaaa", "age": 105, "meta": "data" });
        let payload =
            EncryptedPayload::from_cleartext(&key, serde_json::to_string(&payload_json).unwrap())
                .unwrap();

        let record = payload.decrypt_into::<TestStruct>(&key).unwrap();
        assert_eq!(record.id, "aaaaaaaaaaaa");
        assert_eq!(record.age, 105);
        assert_eq!(record.meta, "data");

        // While we're here, check on EncryptedPayload::serialized_len
        let val_rec = serde_json::to_string(&serde_json::to_value(&payload).unwrap()).unwrap();
        assert_eq!(payload.serialized_len(), val_rec.len());
    }

    #[test]
    fn test_record_bad_hmac() {
        let key1 = KeyBundle::new_random().unwrap();
        let json = json!({ "id": "aaaaaaaaaaaa", "deleted": true, });

        let payload =
            EncryptedPayload::from_cleartext(&key1, serde_json::to_string(&json).unwrap()).unwrap();

        let key2 = KeyBundle::new_random().unwrap();
        let e = payload
            .decrypt(&key2)
            .expect_err("Should fail because wrong keybundle");

        // Note: ErrorKind isn't PartialEq, so.
        assert!(matches!(e, error::Error::CryptoError(_)));
    }
}
