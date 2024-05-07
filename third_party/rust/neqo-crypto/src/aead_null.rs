// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt;

use crate::{
    constants::{Cipher, Version},
    err::{sec::SEC_ERROR_BAD_DATA, Error, Res},
    p11::SymKey,
};

pub const AEAD_NULL_TAG: &[u8] = &[0x0a; 16];

pub struct AeadNull {}

impl AeadNull {
    #[allow(clippy::missing_errors_doc)]
    pub fn new(_version: Version, _cipher: Cipher, _secret: &SymKey, _prefix: &str) -> Res<Self> {
        Ok(Self {})
    }

    #[must_use]
    pub fn expansion(&self) -> usize {
        AEAD_NULL_TAG.len()
    }

    #[allow(clippy::missing_errors_doc)]
    pub fn encrypt<'a>(
        &self,
        _count: u64,
        _aad: &[u8],
        input: &[u8],
        output: &'a mut [u8],
    ) -> Res<&'a [u8]> {
        let l = input.len();
        output[..l].copy_from_slice(input);
        output[l..l + 16].copy_from_slice(AEAD_NULL_TAG);
        Ok(&output[..l + 16])
    }

    #[allow(clippy::missing_errors_doc)]
    pub fn decrypt<'a>(
        &self,
        _count: u64,
        _aad: &[u8],
        input: &[u8],
        output: &'a mut [u8],
    ) -> Res<&'a [u8]> {
        if input.len() < AEAD_NULL_TAG.len() {
            return Err(Error::from(SEC_ERROR_BAD_DATA));
        }

        let len_encrypted = input.len() - AEAD_NULL_TAG.len();
        // Check that:
        // 1) expansion is all zeros and
        // 2) if the encrypted data is also supplied that at least some values are no zero
        //    (otherwise padding will be interpreted as a valid packet)
        if &input[len_encrypted..] == AEAD_NULL_TAG
            && (len_encrypted == 0 || input[..len_encrypted].iter().any(|x| *x != 0x0))
        {
            output[..len_encrypted].copy_from_slice(&input[..len_encrypted]);
            Ok(&output[..len_encrypted])
        } else {
            Err(Error::from(SEC_ERROR_BAD_DATA))
        }
    }
}

impl fmt::Debug for AeadNull {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "[NULL AEAD]")
    }
}
