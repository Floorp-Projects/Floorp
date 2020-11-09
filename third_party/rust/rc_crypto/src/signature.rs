/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains code that was copied from the ring crate which is under
// the ISC license, reproduced below:

// Copyright 2015-2017 Brian Smith.

// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.

// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use crate::Result;
use nss::{ec::Curve, ec::PublicKey, pbkdf2::HashAlgorithm};

/// A signature verification algorithm.
pub struct VerificationAlgorithm {
    curve: Curve,
    digest_alg: HashAlgorithm,
}

pub static ECDSA_P256_SHA256: VerificationAlgorithm = VerificationAlgorithm {
    curve: Curve::P256,
    digest_alg: HashAlgorithm::SHA256,
};

pub static ECDSA_P384_SHA384: VerificationAlgorithm = VerificationAlgorithm {
    curve: Curve::P384,
    digest_alg: HashAlgorithm::SHA384,
};

/// An unparsed public key for signature operations.
pub struct UnparsedPublicKey<'a> {
    alg: &'static VerificationAlgorithm,
    bytes: &'a [u8],
}

impl<'a> UnparsedPublicKey<'a> {
    pub fn new(algorithm: &'static VerificationAlgorithm, bytes: &'a [u8]) -> Self {
        Self {
            alg: algorithm,
            bytes,
        }
    }

    pub fn verify(&self, message: &[u8], signature: &[u8]) -> Result<()> {
        let pub_key = PublicKey::from_bytes(self.alg.curve, self.bytes)?;
        Ok(pub_key.verify(message, signature, self.alg.digest_alg)?)
    }

    pub fn algorithm(&self) -> &'static VerificationAlgorithm {
        self.alg
    }

    pub fn bytes(&self) -> &'a [u8] {
        &self.bytes
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ecdsa_p384_sha384_verify() {
        // Test generated with JS DOM's WebCrypto.
        let pub_key_bytes = base64::decode_config(
            "BMZj_xHOfLQn5DIEQcYUkyASDWo8O30gWdkWXHHHWN5owKhGWplYHEb4PLf3DkFTg_smprr-ApdULy3NV10x8IZ0EfVaUZdXvTquH1kiw2PxD7fhqiozMXUaSuZI5KBE6w",
            base64::URL_SAFE_NO_PAD
        ).unwrap();
        let message = base64::decode_config(
            "F9MQDmEEdvOfm-NkCRrXqG-aVA9kq0xqtjvtWLndmmt6bO2gfLE2CVDDLzJYds0n88uz27c5JkzdsLpm5HP3aLFgD8bgnGm-EgdBpm99CRiIm7mAMbb0-NRAyUxeoGmdgJPVQLWFNoHRwzKV2wZ0Bk-Bq7jkeDHmDfnx-CJKVMQ",
            base64::URL_SAFE_NO_PAD,
        )
        .unwrap();
        let signature = base64::decode_config(
            "XLZmtJweW4qx0u0l6EpfmB5z-S-CNj4mrl9d7U0MuftdNPhmlNacV4AKR-i4uNn0TUIycU7GsfIjIqxuiL9WdAnfq_KH_SJ95mduqXgWNKlyt8JgMLd4h-jKOllh4erh",
            base64::URL_SAFE_NO_PAD,
        )
        .unwrap();
        let public_key =
            crate::signature::UnparsedPublicKey::new(&ECDSA_P384_SHA384, &pub_key_bytes);

        // Failure case: Wrong key algorithm.
        let public_key_wrong_alg =
            crate::signature::UnparsedPublicKey::new(&ECDSA_P256_SHA256, &pub_key_bytes);
        assert!(public_key_wrong_alg.verify(&message, &signature).is_err());

        // Failure case: Add garbage to signature.
        let mut garbage_signature = signature.clone();
        garbage_signature.push(42);
        assert!(public_key.verify(&message, &garbage_signature).is_err());

        // Failure case: Flip a bit in message.
        let mut garbage_message = message.clone();
        garbage_message[42] = 42;
        assert!(public_key.verify(&garbage_message, &signature).is_err());

        // Happy case.
        assert!(public_key.verify(&message, &signature).is_ok());
    }
}
