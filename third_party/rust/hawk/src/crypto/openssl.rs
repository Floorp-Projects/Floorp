use super::{CryptoError, Cryptographer, Hasher, HmacKey};
use crate::DigestAlgorithm;
use std::convert::{TryFrom, TryInto};

use openssl::error::ErrorStack;
use openssl::hash::MessageDigest;
use openssl::pkey::{PKey, Private};
use openssl::sign::Signer;

impl From<ErrorStack> for CryptoError {
    fn from(e: ErrorStack) -> Self {
        CryptoError::Other(e.into())
    }
}

pub struct OpensslCryptographer;

struct OpensslHmacKey {
    key: PKey<Private>,
    digest: MessageDigest,
}

impl HmacKey for OpensslHmacKey {
    fn sign(&self, data: &[u8]) -> Result<Vec<u8>, CryptoError> {
        let mut hmac_signer = Signer::new(self.digest, &self.key)?;
        hmac_signer.update(&data)?;
        let digest = hmac_signer.sign_to_vec()?;
        let mut mac = vec![0; self.digest.size()];
        mac.clone_from_slice(digest.as_ref());
        Ok(mac)
    }
}

// This is always `Some` until `finish` is called.
struct OpensslHasher(Option<openssl::hash::Hasher>);

impl Hasher for OpensslHasher {
    fn update(&mut self, data: &[u8]) -> Result<(), CryptoError> {
        self.0
            .as_mut()
            .expect("update called after `finish`")
            .update(data)?;
        Ok(())
    }

    fn finish(&mut self) -> Result<Vec<u8>, CryptoError> {
        let digest = self.0.take().expect("`finish` called twice").finish()?;
        let bytes: &[u8] = digest.as_ref();
        Ok(bytes.to_owned())
    }
}

impl Cryptographer for OpensslCryptographer {
    fn rand_bytes(&self, output: &mut [u8]) -> Result<(), CryptoError> {
        openssl::rand::rand_bytes(output)?;
        Ok(())
    }

    fn new_key(
        &self,
        algorithm: DigestAlgorithm,
        key: &[u8],
    ) -> Result<Box<dyn HmacKey>, CryptoError> {
        let digest = algorithm.try_into()?;
        Ok(Box::new(OpensslHmacKey {
            key: PKey::hmac(key)?,
            digest,
        }))
    }

    fn constant_time_compare(&self, a: &[u8], b: &[u8]) -> bool {
        // openssl::memcmp::eq panics if the lengths are not the same. ring
        // returns `Err` (and notes in the docs that it is not constant time if
        // the lengths are not the same). We make this behave like ring.
        if a.len() != b.len() {
            false
        } else {
            openssl::memcmp::eq(a, b)
        }
    }

    fn new_hasher(&self, algorithm: DigestAlgorithm) -> Result<Box<dyn Hasher>, CryptoError> {
        let ctx = openssl::hash::Hasher::new(algorithm.try_into()?)?;
        Ok(Box::new(OpensslHasher(Some(ctx))))
    }
}

impl TryFrom<DigestAlgorithm> for MessageDigest {
    type Error = CryptoError;
    fn try_from(algorithm: DigestAlgorithm) -> Result<Self, CryptoError> {
        match algorithm {
            DigestAlgorithm::Sha256 => Ok(MessageDigest::sha256()),
            DigestAlgorithm::Sha384 => Ok(MessageDigest::sha384()),
            DigestAlgorithm::Sha512 => Ok(MessageDigest::sha512()),
            algo => Err(CryptoError::UnsupportedDigest(algo)),
        }
    }
}
