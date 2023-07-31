// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::constants::{Cipher, Version};
use crate::err::{Error, Res};
use crate::p11::{random, SymKey};
use crate::{hkdf, Aead};

use neqo_common::{hex, qinfo, qtrace, Encoder};

use std::mem;

#[derive(Debug)]
pub struct SelfEncrypt {
    version: Version,
    cipher: Cipher,
    key_id: u8,
    key: SymKey,
    old_key: Option<SymKey>,
}

impl SelfEncrypt {
    const VERSION: u8 = 1;
    const SALT_LENGTH: usize = 16;

    /// # Errors
    /// Failure to generate a new HKDF key using NSS results in an error.
    pub fn new(version: Version, cipher: Cipher) -> Res<Self> {
        let key = hkdf::generate_key(version, cipher)?;
        Ok(Self {
            version,
            cipher,
            key_id: 0,
            key,
            old_key: None,
        })
    }

    fn make_aead(&self, k: &SymKey, salt: &[u8]) -> Res<Aead> {
        debug_assert_eq!(salt.len(), Self::SALT_LENGTH);
        let salt = hkdf::import_key(self.version, salt)?;
        let secret = hkdf::extract(self.version, self.cipher, Some(&salt), k)?;
        Aead::new(false, self.version, self.cipher, &secret, "neqo self")
    }

    /// Rotate keys.  This causes any previous key that is being held to be replaced by the current key.
    ///
    /// # Errors
    /// Failure to generate a new HKDF key using NSS results in an error.
    pub fn rotate(&mut self) -> Res<()> {
        let new_key = hkdf::generate_key(self.version, self.cipher)?;
        self.old_key = Some(mem::replace(&mut self.key, new_key));
        let (kid, _) = self.key_id.overflowing_add(1);
        self.key_id = kid;
        qinfo!(["SelfEncrypt"], "Rotated keys to {}", self.key_id);
        Ok(())
    }

    /// Seal an item using the underlying key.  This produces a single buffer that contains
    /// the encrypted `plaintext`, plus a version number and salt.
    /// `aad` is only used as input to the AEAD, it is not included in the output; the
    /// caller is responsible for carrying the AAD as appropriate.
    ///
    /// # Errors
    /// Failure to protect using NSS AEAD APIs produces an error.
    pub fn seal(&self, aad: &[u8], plaintext: &[u8]) -> Res<Vec<u8>> {
        // Format is:
        // struct {
        //   uint8 version;
        //   uint8 key_id;
        //   uint8 salt[16];
        //   opaque aead_encrypted(plaintext)[length as expanded];
        // };
        // AAD covers the entire header, plus the value of the AAD parameter that is provided.
        let salt = random(Self::SALT_LENGTH);
        let cipher = self.make_aead(&self.key, &salt)?;
        let encoded_len = 2 + salt.len() + plaintext.len() + cipher.expansion();

        let mut enc = Encoder::with_capacity(encoded_len);
        enc.encode_byte(Self::VERSION);
        enc.encode_byte(self.key_id);
        enc.encode(&salt);

        let mut extended_aad = enc.clone();
        extended_aad.encode(aad);

        let offset = enc.len();
        let mut output: Vec<u8> = enc.into();
        output.resize(encoded_len, 0);
        cipher.encrypt(0, extended_aad.as_ref(), plaintext, &mut output[offset..])?;
        qtrace!(
            ["SelfEncrypt"],
            "seal {} {} -> {}",
            hex(aad),
            hex(plaintext),
            hex(&output)
        );
        Ok(output)
    }

    fn select_key(&self, kid: u8) -> Option<&SymKey> {
        if kid == self.key_id {
            Some(&self.key)
        } else {
            let (prev_key_id, _) = self.key_id.overflowing_sub(1);
            if kid == prev_key_id {
                self.old_key.as_ref()
            } else {
                None
            }
        }
    }

    /// Open the protected `ciphertext`.
    ///
    /// # Errors
    /// Returns an error when the self-encrypted object is invalid;
    /// when the keys have been rotated; or when NSS fails.
    #[allow(clippy::similar_names)] // aad is similar to aead
    pub fn open(&self, aad: &[u8], ciphertext: &[u8]) -> Res<Vec<u8>> {
        if ciphertext[0] != Self::VERSION {
            return Err(Error::SelfEncryptFailure);
        }
        let Some(key) = self.select_key(ciphertext[1]) else {
            return Err(Error::SelfEncryptFailure);
        };
        let offset = 2 + Self::SALT_LENGTH;

        let mut extended_aad = Encoder::with_capacity(offset + aad.len());
        extended_aad.encode(&ciphertext[0..offset]);
        extended_aad.encode(aad);

        let aead = self.make_aead(key, &ciphertext[2..offset])?;
        // NSS insists on having extra space available for decryption.
        let padded_len = ciphertext.len() - offset;
        let mut output = vec![0; padded_len];
        let decrypted =
            aead.decrypt(0, extended_aad.as_ref(), &ciphertext[offset..], &mut output)?;
        let final_len = decrypted.len();
        output.truncate(final_len);
        qtrace!(
            ["SelfEncrypt"],
            "open {} {} -> {}",
            hex(aad),
            hex(ciphertext),
            hex(&output)
        );
        Ok(output)
    }
}
