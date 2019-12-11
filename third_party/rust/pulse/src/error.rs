// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use std::ffi::CStr;

#[macro_export]
macro_rules! error_result {
    ($t:expr, $err:expr) => {
        if $err >= 0 {
            Ok($t)
        } else {
            Err(ErrorCode::from_error_result($err))
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct ErrorCode {
    err: ffi::pa_error_code_t,
}

impl ErrorCode {
    pub fn from_error_result(err: i32) -> Self {
        debug_assert!(err < 0);
        ErrorCode {
            err: (-err) as ffi::pa_error_code_t,
        }
    }

    pub fn from_error_code(err: ffi::pa_error_code_t) -> Self {
        debug_assert!(err > 0);
        ErrorCode {
            err: err,
        }
    }

    fn desc(&self) -> &'static str {
        let cstr = unsafe { CStr::from_ptr(ffi::pa_strerror(self.err)) };
        cstr.to_str().unwrap()
    }
}

impl ::std::error::Error for ErrorCode {
    fn description(&self) -> &str {
        self.desc()
    }
}

impl ::std::fmt::Display for ErrorCode {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?}: {}", self, self.desc())
    }
}
