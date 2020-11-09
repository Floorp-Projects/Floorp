use super::Cryptographer;
use once_cell::sync::OnceCell;

static CRYPTOGRAPHER: OnceCell<&'static dyn Cryptographer> = OnceCell::new();

#[derive(Debug, thiserror::Error)]
#[error("Cryptographer already initialized")]
pub struct SetCryptographerError(());

/// Sets the global object that will be used for cryptographic operations.
///
/// This is a convenience wrapper over [`set_cryptographer`],
/// but takes a `Box<dyn Cryptographer>` instead.
#[cfg(not(any(feature = "use_ring", feature = "use_openssl")))]
pub fn set_boxed_cryptographer(c: Box<dyn Cryptographer>) -> Result<(), SetCryptographerError> {
    // Just leak the Box. It wouldn't be freed as a `static` anyway, and we
    // never allow this to be re-assigned (so it's not a meaningful memory leak).
    set_cryptographer(Box::leak(c))
}

/// Sets the global object that will be used for cryptographic operations.
///
/// This function may only be called once in the lifetime of a program.
///
/// Any calls into this crate that perform cryptography prior to calling this
/// function will panic.
pub fn set_cryptographer(c: &'static dyn Cryptographer) -> Result<(), SetCryptographerError> {
    CRYPTOGRAPHER.set(c).map_err(|_| SetCryptographerError(()))
}

pub(crate) fn get_crypographer() -> &'static dyn Cryptographer {
    autoinit_crypto();
    *CRYPTOGRAPHER
        .get()
        .expect("`hawk` cryptographer not initialized!")
}

#[cfg(feature = "use_ring")]
#[inline]
fn autoinit_crypto() {
    let _ = set_cryptographer(&super::ring::RingCryptographer);
}

#[cfg(feature = "use_openssl")]
#[inline]
fn autoinit_crypto() {
    let _ = set_cryptographer(&super::openssl::OpensslCryptographer);
}

#[cfg(not(any(feature = "use_openssl", feature = "use_ring")))]
#[inline]
fn autoinit_crypto() {}
