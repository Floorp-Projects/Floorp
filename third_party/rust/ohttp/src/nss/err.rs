// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(
    dead_code,
    clippy::upper_case_acronyms,
    clippy::module_name_repetitions
)]

use super::{SECStatus, SECSuccess};
use crate::err::Res;
use std::os::raw::c_char;

include!(concat!(env!("OUT_DIR"), "/nspr_error.rs"));
mod codes {
    #![allow(non_snake_case)]
    include!(concat!(env!("OUT_DIR"), "/nss_secerr.rs"));
}
pub use codes::SECErrorCodes as sec;
pub mod nspr {
    include!(concat!(env!("OUT_DIR"), "/nspr_err.rs"));
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    name: String,
    code: PRErrorCode,
    desc: String,
}

impl Error {
    /// Get an internal error.
    pub(crate) fn internal() -> Self {
        Self::from(sec::SEC_ERROR_LIBRARY_FAILURE)
    }

    /// Get the last error, as returned by `PR_GetError()`.
    pub(crate) fn last() -> crate::Error {
        crate::Error::from(Self::from(unsafe { PR_GetError() }))
    }
}

impl From<PRErrorCode> for Error {
    fn from(code: PRErrorCode) -> Self {
        let name = wrap_str_fn(|| unsafe { PR_ErrorToName(code) }, "UNKNOWN_ERROR");
        let desc = wrap_str_fn(
            || unsafe { PR_ErrorToString(code, PR_LANGUAGE_I_DEFAULT) },
            "...",
        );
        Error { name, code, desc }
    }
}

impl std::error::Error for Error {}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Error {} ({}): {}", self.name, self.code, self.desc)
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
        Err(Error::last())
    }
}

#[cfg(test)]
mod tests {
    use super::{
        super::{init, SECFailure, SECSuccess},
        secstatus_to_res, PRErrorCode, PR_SetError,
    };

    fn set_error_code(code: PRErrorCode) {
        // This code doesn't work without initializing NSS first.
        init();
        unsafe {
            PR_SetError(code, 0);
        }
    }

    #[test]
    fn error_code() {
        init();
        assert_eq!(166 - 0x2000, super::sec::SEC_ERROR_LIBPKIX_INTERNAL);
        assert_eq!(-5998, super::nspr::PR_WOULD_BLOCK_ERROR);
    }

    #[test]
    fn is_ok() {
        assert!(secstatus_to_res(SECSuccess).is_ok());
    }

    #[test]
    fn is_err() {
        set_error_code(super::sec::SEC_ERROR_BAD_DATABASE);
        let r = secstatus_to_res(SECFailure);
        assert!(r.is_err());
        if let crate::Error::Crypto(e) = r.unwrap_err() {
            assert_eq!(e.name, "SEC_ERROR_BAD_DATABASE");
            assert_eq!(e.code, 18 - 0x2000);
            assert_eq!(e.desc, "security library: bad database.");
        } else {
            panic!();
        }
    }

    #[test]
    fn is_err_zero_code() {
        set_error_code(0);
        let r = secstatus_to_res(SECFailure);
        assert!(r.is_err());
        if let crate::Error::Crypto(e) = r.unwrap_err() {
            assert_eq!(e.name, "UNKNOWN_ERROR");
            assert_eq!(e.code, 0);
        } else {
            panic!();
        }
    }
}
