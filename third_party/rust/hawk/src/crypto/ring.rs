use super::{CryptoError, Cryptographer, Hasher, HmacKey};
use crate::DigestAlgorithm;
use ring::{digest, hmac};
use std::convert::{TryFrom, TryInto};

impl From<ring::error::Unspecified> for CryptoError {
    // Ring's errors are entirely opaque
    fn from(_: ring::error::Unspecified) -> Self {
        CryptoError::Other(anyhow::Error::msg("Unspecified ring error"))
    }
}

impl From<std::convert::Infallible> for CryptoError {
    fn from(_: std::convert::Infallible) -> Self {
        unreachable!()
    }
}

pub struct RingCryptographer;

struct RingHmacKey(hmac::Key);

impl HmacKey for RingHmacKey {
    fn sign(&self, data: &[u8]) -> Result<Vec<u8>, CryptoError> {
        let digest = hmac::sign(&self.0, data);
        let mut mac = vec![0; self.0.algorithm().digest_algorithm().output_len];
        mac.copy_from_slice(digest.as_ref());
        Ok(mac)
    }
}
// This is always `Some` until `finish` is called.
struct RingHasher(Option<digest::Context>);

impl Hasher for RingHasher {
    fn update(&mut self, data: &[u8]) -> Result<(), CryptoError> {
        self.0
            .as_mut()
            .expect("update called after `finish`")
            .update(data);
        Ok(())
    }

    fn finish(&mut self) -> Result<Vec<u8>, CryptoError> {
        let digest = self.0.take().expect("`finish` called twice").finish();
        let bytes: &[u8] = digest.as_ref();
        Ok(bytes.to_owned())
    }
}

impl Cryptographer for RingCryptographer {
    fn rand_bytes(&self, output: &mut [u8]) -> Result<(), CryptoError> {
        use ring::rand::SecureRandom;
        let rnd = ring::rand::SystemRandom::new();
        rnd.fill(output)?;
        Ok(())
    }

    fn new_key(
        &self,
        algorithm: DigestAlgorithm,
        key: &[u8],
    ) -> Result<Box<dyn HmacKey>, CryptoError> {
        let k = hmac::Key::new(algorithm.try_into()?, key);
        Ok(Box::new(RingHmacKey(k)))
    }

    fn constant_time_compare(&self, a: &[u8], b: &[u8]) -> bool {
        ring::constant_time::verify_slices_are_equal(a, b).is_ok()
    }

    fn new_hasher(&self, algorithm: DigestAlgorithm) -> Result<Box<dyn Hasher>, CryptoError> {
        let ctx = digest::Context::new(algorithm.try_into()?);
        Ok(Box::new(RingHasher(Some(ctx))))
    }
}

impl TryFrom<DigestAlgorithm> for &'static digest::Algorithm {
    type Error = CryptoError;
    fn try_from(algorithm: DigestAlgorithm) -> Result<Self, CryptoError> {
        match algorithm {
            DigestAlgorithm::Sha256 => Ok(&digest::SHA256),
            DigestAlgorithm::Sha384 => Ok(&digest::SHA384),
            DigestAlgorithm::Sha512 => Ok(&digest::SHA512),
            algo => Err(CryptoError::UnsupportedDigest(algo)),
        }
    }
}

impl TryFrom<DigestAlgorithm> for hmac::Algorithm {
    type Error = CryptoError;
    fn try_from(algorithm: DigestAlgorithm) -> Result<Self, CryptoError> {
        match algorithm {
            DigestAlgorithm::Sha256 => Ok(hmac::HMAC_SHA256),
            DigestAlgorithm::Sha384 => Ok(hmac::HMAC_SHA384),
            DigestAlgorithm::Sha512 => Ok(hmac::HMAC_SHA512),
            algo => Err(CryptoError::UnsupportedDigest(algo)),
        }
    }
}
