//! `hawk` must perform certain cryptographic operations in order to function,
//! and applications may need control over which library is used for these.
//!
//! This module can be used for that purpose. If you do not care, this crate can
//! be configured so that a default implementation is provided based on either
//! `ring` or `openssl` (via the `use_ring` and `use_openssl` features respectively).
//!
//! Should you need something custom, then you can provide it by implementing
//! [`Cryptographer`] and using the [`set_cryptographer`] or
//! [`set_boxed_cryptographer`] functions.
use crate::DigestAlgorithm;

pub(crate) mod holder;
pub(crate) use holder::get_crypographer;

#[cfg(feature = "use_openssl")]
mod openssl;
#[cfg(feature = "use_ring")]
mod ring;

#[cfg(not(any(feature = "use_ring", feature = "use_openssl")))]
pub use self::holder::{set_boxed_cryptographer, set_cryptographer};

#[derive(Debug, thiserror::Error)]
pub enum CryptoError {
    /// The configured cryptographer does not support the digest algorithm
    /// specified. This should only happen for custom `Cryptographer` implementations
    #[error("Digest algorithm {0:?} is unsupported by this Cryptographer")]
    UnsupportedDigest(DigestAlgorithm),

    /// The configured cryptographer implementation failed to perform an
    /// operation in some way.
    #[error("{0}")]
    Other(#[source] anyhow::Error),
}

/// A trait encapsulating the cryptographic operations required by this library.
///
/// If you use this library with either the `use_ring` or `use_openssl` features enabled,
/// then you do not have to worry about this.
pub trait Cryptographer: Send + Sync + 'static {
    fn rand_bytes(&self, output: &mut [u8]) -> Result<(), CryptoError>;
    fn new_key(
        &self,
        algorithm: DigestAlgorithm,
        key: &[u8],
    ) -> Result<Box<dyn HmacKey>, CryptoError>;
    fn new_hasher(&self, algo: DigestAlgorithm) -> Result<Box<dyn Hasher>, CryptoError>;
    fn constant_time_compare(&self, a: &[u8], b: &[u8]) -> bool;
}

/// Type-erased hmac key type.
pub trait HmacKey: Send + Sync + 'static {
    fn sign(&self, data: &[u8]) -> Result<Vec<u8>, CryptoError>;
}

/// Type-erased hash context type.
pub trait Hasher: Send + Sync + 'static {
    fn update(&mut self, data: &[u8]) -> Result<(), CryptoError>;
    // Note: this would take by move but that's not object safe :(
    fn finish(&mut self) -> Result<Vec<u8>, CryptoError>;
}

// For convenience

pub(crate) fn rand_bytes(buffer: &mut [u8]) -> Result<(), CryptoError> {
    get_crypographer().rand_bytes(buffer)
}

pub(crate) fn new_key(
    algorithm: DigestAlgorithm,
    key: &[u8],
) -> Result<Box<dyn HmacKey>, CryptoError> {
    get_crypographer().new_key(algorithm, key)
}

pub(crate) fn constant_time_compare(a: &[u8], b: &[u8]) -> bool {
    get_crypographer().constant_time_compare(a, b)
}

pub(crate) fn new_hasher(algorithm: DigestAlgorithm) -> Result<Box<dyn Hasher>, CryptoError> {
    Ok(get_crypographer().new_hasher(algorithm)?)
}
