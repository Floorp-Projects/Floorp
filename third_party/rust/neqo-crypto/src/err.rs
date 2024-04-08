// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]
#![allow(clippy::upper_case_acronyms)]

use std::{os::raw::c_char, str::Utf8Error};

use crate::ssl::{SECStatus, SECSuccess};

include!(concat!(env!("OUT_DIR"), "/nspr_error.rs"));
mod codes {
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/nss_secerr.rs"));
    include!(concat!(env!("OUT_DIR"), "/nss_sslerr.rs"));
}
pub use codes::{SECErrorCodes as sec, SSLErrorCodes as ssl};
pub mod nspr {
    include!(concat!(env!("OUT_DIR"), "/nspr_err.rs"));
}

pub mod mozpkix {
    // These are manually extracted from the many bindings generated
    // by bindgen when provided with the simple header:
    // #include "mozpkix/pkixnss.h"

    #[allow(non_camel_case_types)]
    pub type mozilla_pkix_ErrorCode = ::std::os::raw::c_int;
    pub const MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE: mozilla_pkix_ErrorCode = -16384;
    pub const MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY: mozilla_pkix_ErrorCode = -16383;
    pub const MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE: mozilla_pkix_ErrorCode = -16382;
    pub const MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA: mozilla_pkix_ErrorCode = -16381;
    pub const MOZILLA_PKIX_ERROR_NO_RFC822NAME_MATCH: mozilla_pkix_ErrorCode = -16380;
    pub const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE: mozilla_pkix_ErrorCode = -16379;
    pub const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE: mozilla_pkix_ErrorCode = -16378;
    pub const MOZILLA_PKIX_ERROR_SIGNATURE_ALGORITHM_MISMATCH: mozilla_pkix_ErrorCode = -16377;
    pub const MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING: mozilla_pkix_ErrorCode = -16376;
    pub const MOZILLA_PKIX_ERROR_VALIDITY_TOO_LONG: mozilla_pkix_ErrorCode = -16375;
    pub const MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING: mozilla_pkix_ErrorCode = -16374;
    pub const MOZILLA_PKIX_ERROR_INVALID_INTEGER_ENCODING: mozilla_pkix_ErrorCode = -16373;
    pub const MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME: mozilla_pkix_ErrorCode = -16372;
    pub const MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED: mozilla_pkix_ErrorCode =
        -16371;
    pub const MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT: mozilla_pkix_ErrorCode = -16370;
    pub const MOZILLA_PKIX_ERROR_MITM_DETECTED: mozilla_pkix_ErrorCode = -16369;
    pub const END_OF_LIST: mozilla_pkix_ErrorCode = -16368;
}

pub type Res<T> = Result<T, Error>;

#[derive(Clone, Debug, PartialEq, PartialOrd, Ord, Eq)]
pub enum Error {
    AeadError,
    CertificateLoading,
    CipherInitFailure,
    CreateSslSocket,
    EchRetry(Vec<u8>),
    HkdfError,
    InternalError,
    IntegerOverflow,
    InvalidEpoch,
    MixedHandshakeMethod,
    NoDataAvailable,
    NssError {
        name: String,
        code: PRErrorCode,
        desc: String,
    },
    OverrunError,
    SelfEncryptFailure,
    StringError,
    TimeTravelError,
    UnsupportedCipher,
    UnsupportedVersion,
}

impl Error {
    pub(crate) fn last_nss_error() -> Self {
        Self::from(unsafe { PR_GetError() })
    }
}

impl std::error::Error for Error {
    #[must_use]
    fn cause(&self) -> Option<&dyn std::error::Error> {
        None
    }
    #[must_use]
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        None
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Error: {self:?}")
    }
}

impl From<std::num::TryFromIntError> for Error {
    #[must_use]
    fn from(_: std::num::TryFromIntError) -> Self {
        Self::IntegerOverflow
    }
}
impl From<std::ffi::NulError> for Error {
    #[must_use]
    fn from(_: std::ffi::NulError) -> Self {
        Self::InternalError
    }
}
impl From<Utf8Error> for Error {
    fn from(_: Utf8Error) -> Self {
        Self::StringError
    }
}
impl From<PRErrorCode> for Error {
    fn from(code: PRErrorCode) -> Self {
        let name = wrap_str_fn(|| unsafe { PR_ErrorToName(code) }, "UNKNOWN_ERROR");
        let desc = wrap_str_fn(
            || unsafe { PR_ErrorToString(code, PR_LANGUAGE_I_DEFAULT) },
            "...",
        );
        Self::NssError { name, code, desc }
    }
}

use std::ffi::CStr;

fn wrap_str_fn<F>(f: F, dflt: &str) -> String
where
    F: FnOnce() -> *const c_char,
{
    unsafe {
        let p = f();
        if p.is_null() {
            return dflt.to_string();
        }
        CStr::from_ptr(p).to_string_lossy().into_owned()
    }
}

pub fn secstatus_to_res(rv: SECStatus) -> Res<()> {
    if rv == SECSuccess {
        Ok(())
    } else {
        Err(Error::last_nss_error())
    }
}

pub fn is_blocked(result: &Res<()>) -> bool {
    match result {
        Err(Error::NssError { code, .. }) => *code == nspr::PR_WOULD_BLOCK_ERROR,
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use test_fixture::fixture_init;

    use crate::{
        err::{self, is_blocked, secstatus_to_res, Error, PRErrorCode, PR_SetError},
        ssl::{SECFailure, SECSuccess},
    };

    fn set_error_code(code: PRErrorCode) {
        // This code doesn't work without initializing NSS first.
        fixture_init();
        unsafe {
            PR_SetError(code, 0);
        }
    }

    #[test]
    fn error_code() {
        fixture_init();
        assert_eq!(15 - 0x3000, err::ssl::SSL_ERROR_BAD_MAC_READ);
        assert_eq!(166 - 0x2000, err::sec::SEC_ERROR_LIBPKIX_INTERNAL);
        assert_eq!(-5998, err::nspr::PR_WOULD_BLOCK_ERROR);
    }

    #[test]
    fn is_ok() {
        assert!(secstatus_to_res(SECSuccess).is_ok());
    }

    #[test]
    fn is_err() {
        set_error_code(err::ssl::SSL_ERROR_BAD_MAC_READ);
        let r = secstatus_to_res(SECFailure);
        assert!(r.is_err());
        match r.unwrap_err() {
            Error::NssError { name, code, desc } => {
                assert_eq!(name, "SSL_ERROR_BAD_MAC_READ");
                assert_eq!(code, -12273);
                assert_eq!(
                    desc,
                    "SSL received a record with an incorrect Message Authentication Code."
                );
            }
            _ => unreachable!(),
        }
    }

    #[test]
    fn is_err_zero_code() {
        set_error_code(0);
        let r = secstatus_to_res(SECFailure);
        assert!(r.is_err());
        match r.unwrap_err() {
            Error::NssError { name, code, .. } => {
                assert_eq!(name, "UNKNOWN_ERROR");
                assert_eq!(code, 0);
                // Note that we don't test |desc| here because that comes from
                // strerror(0), which is platform-dependent.
            }
            _ => unreachable!(),
        }
    }

    #[test]
    fn blocked() {
        set_error_code(err::nspr::PR_WOULD_BLOCK_ERROR);
        let r = secstatus_to_res(SECFailure);
        assert!(r.is_err());
        assert!(is_blocked(&r));
        match r.unwrap_err() {
            Error::NssError { name, code, desc } => {
                assert_eq!(name, "PR_WOULD_BLOCK_ERROR");
                assert_eq!(code, -5998);
                assert_eq!(desc, "The operation would have blocked");
            }
            _ => panic!("bad error type"),
        }
    }
}
