#![allow(dead_code)] // TODO: remove

use super::SymKey;
use crate::{err::Res, hpke::Aead as AeadId};
use aead::{AeadMut, Key, NewAead, Nonce, Payload};
use aes_gcm::{Aes128Gcm, Aes256Gcm};
use chacha20poly1305::ChaCha20Poly1305;
use std::convert::TryFrom;

/// All the nonces are the same length.  Exploit that.
pub const NONCE_LEN: usize = 12;
const COUNTER_LEN: usize = 8;
const TAG_LEN: usize = 16;

type SequenceNumber = u64;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Encrypt,
    Decrypt,
}

enum AeadEngine {
    Aes128Gcm(Box<Aes128Gcm>),
    Aes256Gcm(Box<Aes256Gcm>),
    ChaCha20Poly1305(Box<ChaCha20Poly1305>),
}

// Dispatch functions; this just shows how janky that this sort of abstraction can be.
// If this grows too much, this is fairly clearly responsive to using a macro.
impl AeadEngine {
    fn encrypt(&mut self, nonce: &[u8], pt: Payload) -> Res<Vec<u8>> {
        let tag = match self {
            Self::Aes128Gcm(e) => e.encrypt(Nonce::<Aes128Gcm>::from_slice(nonce), pt)?,
            Self::Aes256Gcm(e) => e.encrypt(Nonce::<Aes256Gcm>::from_slice(nonce), pt)?,
            Self::ChaCha20Poly1305(e) => {
                e.encrypt(Nonce::<ChaCha20Poly1305>::from_slice(nonce), pt)?
            }
        };
        Ok(tag)
    }
    fn decrypt(&mut self, nonce: &[u8], pt: Payload) -> Res<Vec<u8>> {
        let tag = match self {
            Self::Aes128Gcm(e) => e.decrypt(Nonce::<Aes128Gcm>::from_slice(nonce), pt)?,
            Self::Aes256Gcm(e) => e.decrypt(Nonce::<Aes256Gcm>::from_slice(nonce), pt)?,
            Self::ChaCha20Poly1305(e) => {
                e.decrypt(Nonce::<ChaCha20Poly1305>::from_slice(nonce), pt)?
            }
        };
        Ok(tag)
    }
}

/// A switch-hitting AEAD that uses a selected primitive.
pub struct Aead {
    mode: Mode,
    aead: AeadEngine,
    nonce_base: [u8; NONCE_LEN],
    seq: SequenceNumber,
}

impl Aead {
    #[allow(clippy::unnecessary_wraps)]
    pub fn new(
        mode: Mode,
        algorithm: AeadId,
        key: &SymKey,
        nonce_base: [u8; NONCE_LEN],
    ) -> Res<Self> {
        let aead = match algorithm {
            AeadId::Aes128Gcm => AeadEngine::Aes128Gcm(Box::new(Aes128Gcm::new(
                Key::<Aes128Gcm>::from_slice(key.as_ref()),
            ))),
            AeadId::Aes256Gcm => AeadEngine::Aes256Gcm(Box::new(Aes256Gcm::new(
                Key::<Aes256Gcm>::from_slice(key.as_ref()),
            ))),
            AeadId::ChaCha20Poly1305 => AeadEngine::ChaCha20Poly1305(Box::new(
                ChaCha20Poly1305::new(Key::<ChaCha20Poly1305>::from_slice(key.as_ref())),
            )),
        };
        Ok(Self {
            mode,
            aead,
            nonce_base,
            seq: 0,
        })
    }

    #[cfg(test)]
    #[allow(clippy::unnecessary_wraps)]
    fn import_key(_alg: AeadId, k: &[u8]) -> Res<SymKey> {
        Ok(SymKey::from(k))
    }

    fn nonce(&self, seq: SequenceNumber) -> Vec<u8> {
        let mut nonce = Vec::from(self.nonce_base);
        for (i, n) in nonce.iter_mut().rev().take(COUNTER_LEN).enumerate() {
            *n ^= u8::try_from((seq >> (8 * i)) & 0xff).unwrap();
        }
        nonce
    }

    pub fn seal(&mut self, aad: &[u8], pt: &[u8]) -> Res<Vec<u8>> {
        assert_eq!(self.mode, Mode::Encrypt);
        // A copy for the nonce generator to write into.  But we don't use the value.
        let nonce = self.nonce(self.seq);
        self.seq += 1;
        let ct = self.aead.encrypt(&nonce, Payload { msg: pt, aad })?;
        Ok(ct)
    }

    pub fn open(&mut self, aad: &[u8], seq: SequenceNumber, ct: &[u8]) -> Res<Vec<u8>> {
        assert_eq!(self.mode, Mode::Decrypt);
        let nonce = self.nonce(seq);
        let pt = self.aead.decrypt(&nonce, Payload { msg: ct, aad })?;
        Ok(pt)
    }
}

#[cfg(test)]
mod test {
    use super::{
        super::super::{hpke::Aead as AeadId, init},
        Aead, Mode, SequenceNumber, NONCE_LEN,
    };

    /// Check that the first invocation of encryption matches expected values.
    /// Also check decryption of the same.
    fn check0(
        algorithm: AeadId,
        key: &[u8],
        nonce: &[u8; NONCE_LEN],
        aad: &[u8],
        pt: &[u8],
        ct: &[u8],
    ) {
        init();
        let k = Aead::import_key(algorithm, key).unwrap();

        let mut enc = Aead::new(Mode::Encrypt, algorithm, &k, *nonce).unwrap();
        let ciphertext = enc.seal(aad, pt).unwrap();
        assert_eq!(&ciphertext[..], ct);

        let mut dec = Aead::new(Mode::Decrypt, algorithm, &k, *nonce).unwrap();
        let plaintext = dec.open(aad, 0, ct).unwrap();
        assert_eq!(&plaintext[..], pt);
    }

    fn decrypt(
        algorithm: AeadId,
        key: &[u8],
        nonce: &[u8; NONCE_LEN],
        seq: SequenceNumber,
        aad: &[u8],
        pt: &[u8],
        ct: &[u8],
    ) {
        let k = Aead::import_key(algorithm, key).unwrap();
        let mut dec = Aead::new(Mode::Decrypt, algorithm, &k, *nonce).unwrap();
        let plaintext = dec.open(aad, seq, ct).unwrap();
        assert_eq!(&plaintext[..], pt);
    }

    /// This tests the AEAD in QUIC in combination with the HKDF code.
    /// This is an AEAD-only example.
    #[test]
    fn quic_retry() {
        const KEY: &[u8] = &[
            0xbe, 0x0c, 0x69, 0x0b, 0x9f, 0x66, 0x57, 0x5a, 0x1d, 0x76, 0x6b, 0x54, 0xe3, 0x68,
            0xc8, 0x4e,
        ];
        const NONCE: &[u8; NONCE_LEN] = &[
            0x46, 0x15, 0x99, 0xd3, 0x5d, 0x63, 0x2b, 0xf2, 0x23, 0x98, 0x25, 0xbb,
        ];
        const AAD: &[u8] = &[
            0x08, 0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08, 0xff, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a, 0x42, 0x62, 0xb5, 0x74, 0x6f, 0x6b, 0x65,
            0x6e,
        ];
        const CT: &[u8] = &[
            0x04, 0xa2, 0x65, 0xba, 0x2e, 0xff, 0x4d, 0x82, 0x90, 0x58, 0xfb, 0x3f, 0x0f, 0x24,
            0x96, 0xba,
        ];
        check0(AeadId::Aes128Gcm, KEY, NONCE, AAD, &[], CT);
    }

    #[test]
    fn quic_server_initial() {
        const ALG: AeadId = AeadId::Aes128Gcm;
        const KEY: &[u8] = &[
            0xcf, 0x3a, 0x53, 0x31, 0x65, 0x3c, 0x36, 0x4c, 0x88, 0xf0, 0xf3, 0x79, 0xb6, 0x06,
            0x7e, 0x37,
        ];
        const NONCE_BASE: &[u8; NONCE_LEN] = &[
            0x0a, 0xc1, 0x49, 0x3c, 0xa1, 0x90, 0x58, 0x53, 0xb0, 0xbb, 0xa0, 0x3e,
        ];
        // Note that this integrates the sequence number of 1 from the example,
        // otherwise we can't use a sequence number of 0 to encrypt.
        const NONCE: &[u8; NONCE_LEN] = &[
            0x0a, 0xc1, 0x49, 0x3c, 0xa1, 0x90, 0x58, 0x53, 0xb0, 0xbb, 0xa0, 0x3f,
        ];
        const AAD: &[u8] = &[
            0xc1, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0xf0, 0x67, 0xa5, 0x50, 0x2a, 0x42, 0x62,
            0xb5, 0x00, 0x40, 0x75, 0x00, 0x01,
        ];
        const PT: &[u8] = &[
            0x02, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x40, 0x5a, 0x02, 0x00, 0x00, 0x56, 0x03,
            0x03, 0xee, 0xfc, 0xe7, 0xf7, 0xb3, 0x7b, 0xa1, 0xd1, 0x63, 0x2e, 0x96, 0x67, 0x78,
            0x25, 0xdd, 0xf7, 0x39, 0x88, 0xcf, 0xc7, 0x98, 0x25, 0xdf, 0x56, 0x6d, 0xc5, 0x43,
            0x0b, 0x9a, 0x04, 0x5a, 0x12, 0x00, 0x13, 0x01, 0x00, 0x00, 0x2e, 0x00, 0x33, 0x00,
            0x24, 0x00, 0x1d, 0x00, 0x20, 0x9d, 0x3c, 0x94, 0x0d, 0x89, 0x69, 0x0b, 0x84, 0xd0,
            0x8a, 0x60, 0x99, 0x3c, 0x14, 0x4e, 0xca, 0x68, 0x4d, 0x10, 0x81, 0x28, 0x7c, 0x83,
            0x4d, 0x53, 0x11, 0xbc, 0xf3, 0x2b, 0xb9, 0xda, 0x1a, 0x00, 0x2b, 0x00, 0x02, 0x03,
            0x04,
        ];
        const CT: &[u8] = &[
            0x5a, 0x48, 0x2c, 0xd0, 0x99, 0x1c, 0xd2, 0x5b, 0x0a, 0xac, 0x40, 0x6a, 0x58, 0x16,
            0xb6, 0x39, 0x41, 0x00, 0xf3, 0x7a, 0x1c, 0x69, 0x79, 0x75, 0x54, 0x78, 0x0b, 0xb3,
            0x8c, 0xc5, 0xa9, 0x9f, 0x5e, 0xde, 0x4c, 0xf7, 0x3c, 0x3e, 0xc2, 0x49, 0x3a, 0x18,
            0x39, 0xb3, 0xdb, 0xcb, 0xa3, 0xf6, 0xea, 0x46, 0xc5, 0xb7, 0x68, 0x4d, 0xf3, 0x54,
            0x8e, 0x7d, 0xde, 0xb9, 0xc3, 0xbf, 0x9c, 0x73, 0xcc, 0x3f, 0x3b, 0xde, 0xd7, 0x4b,
            0x56, 0x2b, 0xfb, 0x19, 0xfb, 0x84, 0x02, 0x2f, 0x8e, 0xf4, 0xcd, 0xd9, 0x37, 0x95,
            0xd7, 0x7d, 0x06, 0xed, 0xbb, 0x7a, 0xaf, 0x2f, 0x58, 0x89, 0x18, 0x50, 0xab, 0xbd,
            0xca, 0x3d, 0x20, 0x39, 0x8c, 0x27, 0x64, 0x56, 0xcb, 0xc4, 0x21, 0x58, 0x40, 0x7d,
            0xd0, 0x74, 0xee,
        ];
        check0(ALG, KEY, NONCE, AAD, PT, CT);
        decrypt(ALG, KEY, NONCE_BASE, 1, AAD, PT, CT);
    }

    #[test]
    fn quic_chacha() {
        const ALG: AeadId = AeadId::ChaCha20Poly1305;
        const KEY: &[u8] = &[
            0xc6, 0xd9, 0x8f, 0xf3, 0x44, 0x1c, 0x3f, 0xe1, 0xb2, 0x18, 0x20, 0x94, 0xf6, 0x9c,
            0xaa, 0x2e, 0xd4, 0xb7, 0x16, 0xb6, 0x54, 0x88, 0x96, 0x0a, 0x7a, 0x98, 0x49, 0x79,
            0xfb, 0x23, 0xe1, 0xc8,
        ];
        const NONCE_BASE: &[u8; NONCE_LEN] = &[
            0xe0, 0x45, 0x9b, 0x34, 0x74, 0xbd, 0xd0, 0xe4, 0x4a, 0x41, 0xc1, 0x44,
        ];
        // Note that this integrates the sequence number of 654360564 from the example,
        // otherwise we can't use a sequence number of 0 to encrypt.
        const NONCE: &[u8; NONCE_LEN] = &[
            0xe0, 0x45, 0x9b, 0x34, 0x74, 0xbd, 0xd0, 0xe4, 0x6d, 0x41, 0x7e, 0xb0,
        ];
        const AAD: &[u8] = &[0x42, 0x00, 0xbf, 0xf4];
        const PT: &[u8] = &[0x01];
        const CT: &[u8] = &[
            0x65, 0x5e, 0x5c, 0xd5, 0x5c, 0x41, 0xf6, 0x90, 0x80, 0x57, 0x5d, 0x79, 0x99, 0xc2,
            0x5a, 0x5b, 0xfb,
        ];
        check0(ALG, KEY, NONCE, AAD, PT, CT);
        // Now use the real nonce and sequence number from the example.
        decrypt(ALG, KEY, NONCE_BASE, 654_360_564, AAD, PT, CT);
    }
}
