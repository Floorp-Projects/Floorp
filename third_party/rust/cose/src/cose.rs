//! This crate implements [COSE](https://tools.ietf.org/html/rfc8152) signature
//! parsing. Verification has to be performed by the caller.
//!
//! Example usage: Let `payload` and `cose_signature` be variables holding the
//! signed payload and the COSE signature bytes respectively.
//! Let further `verify_callback` be a function callback that implements
//! signature verification.
//!
//!```rust,ignore
//! use cose::decoder::decode_signature;
//!
//! // Parse the incoming signature.
//! let cose_signatures = decode_signature(cose_signature, &payload);
//! let cose_signatures = match cose_signatures {
//!     Ok(signature) => signature,
//!     Err(_) => Vec::new(),
//! };
//! if cose_signatures.len() < 1 {
//!     return false;
//! }
//!
//! let mut result = true;
//! for cose_signature in cose_signatures {
//!     // Call callback to verify the parsed signatures.
//!     result &= verify_callback(cose_signature);
//!
//!     // We can stop early. The cose_signature is not valid.
//!     if !result {
//!         return result;
//!     }
//! }
//!```
extern crate moz_cbor as cbor;

pub mod decoder;
pub mod util;

/// Errors that can be returned from COSE functions.
#[derive(Debug, PartialEq)]
pub enum CoseError {
    DecodingFailure,
    LibraryFailure,
    MalformedInput,
    MissingHeader,
    UnexpectedHeaderValue,
    UnexpectedTag,
    UnexpectedType,
    Unimplemented,
    VerificationFailed,
    UnknownSignatureScheme,
    SigningFailed,
    InvalidArgument,
}

/// An enum identifying supported signature algorithms.
/// Currently ES256 (ECDSA with P256 and SHA256), ES384 (ECDSA with P384 and SHA384)
/// ES512 (ECDSA with P521 and SHA512), and PS256 (RSASSA-PSS with SHA256)
/// are supported. Note that with PS256, the salt length is defined
/// to be 32 bytes.
#[derive(Debug)]
#[derive(PartialEq)]
pub enum SignatureAlgorithm {
    ES256,
    ES384,
    ES512,
    PS256,
}

#[cfg(test)]
mod test_setup;
#[cfg(test)]
mod test_cose;
