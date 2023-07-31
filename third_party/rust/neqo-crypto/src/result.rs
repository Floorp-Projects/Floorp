// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{
    err::{nspr, Error, PR_ErrorToName, PR_ErrorToString, PR_GetError, Res, PR_LANGUAGE_I_DEFAULT},
    ssl,
};

use std::ffi::CStr;

pub fn result(rv: ssl::SECStatus) -> Res<()> {
    _ = result_helper(rv, false)?;
    Ok(())
}

pub fn result_or_blocked(rv: ssl::SECStatus) -> Res<bool> {
    result_helper(rv, true)
}

fn wrap_str_fn<F>(f: F, dflt: &str) -> String
where
    F: FnOnce() -> *const i8,
{
    unsafe {
        let p = f();
        if p.is_null() {
            return dflt.to_string();
        }
        CStr::from_ptr(p).to_string_lossy().into_owned()
    }
}

fn result_helper(rv: ssl::SECStatus, allow_blocked: bool) -> Res<bool> {
    if rv == ssl::_SECStatus_SECSuccess {
        return Ok(false);
    }

    let code = unsafe { PR_GetError() };
    if allow_blocked && code == nspr::PR_WOULD_BLOCK_ERROR {
        return Ok(true);
    }

    let name = wrap_str_fn(|| unsafe { PR_ErrorToName(code) }, "UNKNOWN_ERROR");
    let desc = wrap_str_fn(
        || unsafe { PR_ErrorToString(code, PR_LANGUAGE_I_DEFAULT) },
        "...",
    );
    Err(Error::NssError { name, code, desc })
}

#[cfg(test)]
mod tests {
    use super::{result, result_or_blocked};
    use crate::{
        err::{self, nspr, Error, PRErrorCode, PR_SetError},
        ssl,
    };
    use test_fixture::fixture_init;

    fn set_error_code(code: PRErrorCode) {
        unsafe { PR_SetError(code, 0) };
    }

    #[test]
    fn is_ok() {
        assert!(result(ssl::SECSuccess).is_ok());
    }

    #[test]
    fn is_err() {
        // This code doesn't work without initializing NSS first.
        fixture_init();

        set_error_code(err::ssl::SSL_ERROR_BAD_MAC_READ);
        let r = result(ssl::SECFailure);
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
        // This code doesn't work without initializing NSS first.
        fixture_init();

        set_error_code(0);
        let r = result(ssl::SECFailure);
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
    fn blocked_as_error() {
        // This code doesn't work without initializing NSS first.
        fixture_init();

        set_error_code(nspr::PR_WOULD_BLOCK_ERROR);
        let r = result(ssl::SECFailure);
        assert!(r.is_err());
        match r.unwrap_err() {
            Error::NssError { name, code, desc } => {
                assert_eq!(name, "PR_WOULD_BLOCK_ERROR");
                assert_eq!(code, -5998);
                assert_eq!(desc, "The operation would have blocked");
            }
            _ => panic!("bad error type"),
        }
    }

    #[test]
    fn is_blocked() {
        set_error_code(nspr::PR_WOULD_BLOCK_ERROR);
        assert!(result_or_blocked(ssl::SECFailure).unwrap());
    }
}
