// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(dead_code)]

use std::os::raw::c_char;

use crate::ssl::{SECStatus, SECSuccess};

include!(concat!(env!("OUT_DIR"), "/nspr_error.rs"));
mod codes {
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/nss_secerr.rs"));
    include!(concat!(env!("OUT_DIR"), "/nss_sslerr.rs"));
    include!(concat!(env!("OUT_DIR"), "/mozpkix.rs"));
}
pub use codes::mozilla_pkix_ErrorCode as mozpkix;
pub use codes::SECErrorCodes as sec;
pub use codes::SSLErrorCodes as ssl;
pub mod nspr {
    include!(concat!(env!("OUT_DIR"), "/nspr_err.rs"));
}

pub type Res<T> = Result<T, Error>;

#[derive(Clone, Debug, PartialEq, PartialOrd, Ord, Eq)]
#[allow(clippy::pub_enum_variant_names)]
pub enum Error {
    AeadInitFailure,
    AeadError,
    CertificateLoading,
    CreateSslSocket,
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
    TimeTravelError,
    UnsupportedCipher,
    UnsupportedVersion,
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
        write!(f, "Error: {:?}", self)
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
        return Ok(());
    }

    let code = unsafe { PR_GetError() };
    let name = wrap_str_fn(|| unsafe { PR_ErrorToName(code) }, "UNKNOWN_ERROR");
    let desc = wrap_str_fn(
        || unsafe { PR_ErrorToString(code, PR_LANGUAGE_I_DEFAULT) },
        "...",
    );
    Err(Error::NssError { name, code, desc })
}

pub fn is_blocked(result: &Res<()>) -> bool {
    match result {
        Err(Error::NssError { code, .. }) => *code == nspr::PR_WOULD_BLOCK_ERROR,
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use crate::err::{self, is_blocked, secstatus_to_res, Error, PRErrorCode, PR_SetError};
    use crate::ssl::{SECFailure, SECSuccess};
    use test_fixture::fixture_init;

    fn set_error_code(code: PRErrorCode) {
        // This code doesn't work without initializing NSS first.
        fixture_init();
        unsafe { PR_SetError(code, 0) };
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
