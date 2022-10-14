use std::ffi;
use std::ffi::CString;
use std::fmt;
use std::str;

use libc::c_char;
use regex;

#[derive(Debug)]
pub struct Error {
    message: Option<CString>,
    kind: ErrorKind,
}

#[derive(Debug)]
pub enum ErrorKind {
    None,
    Str(str::Utf8Error),
    Regex(regex::Error),
    Nul(ffi::NulError),
}

impl Error {
    pub fn new(kind: ErrorKind) -> Error {
        Error { message: None, kind: kind }
    }

    pub fn is_err(&self) -> bool {
        match self.kind {
            ErrorKind::None => false,
            ErrorKind::Str(_) | ErrorKind::Regex(_) | ErrorKind::Nul(_) => {
                true
            }
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.kind {
            ErrorKind::None => write!(f, "no error"),
            ErrorKind::Str(ref e) => e.fmt(f),
            ErrorKind::Regex(ref e) => e.fmt(f),
            ErrorKind::Nul(ref e) => e.fmt(f),
        }
    }
}

ffi_fn! {
    fn rure_error_new() -> *mut Error {
        Box::into_raw(Box::new(Error::new(ErrorKind::None)))
    }
}

ffi_fn! {
    fn rure_error_free(err: *mut Error) {
        unsafe { drop(Box::from_raw(err)); }
    }
}

ffi_fn! {
    fn rure_error_message(err: *mut Error) -> *const c_char {
        let err = unsafe { &mut *err };
        let cmsg = match CString::new(format!("{}", err)) {
            Ok(msg) => msg,
            Err(err) => {
                // I guess this can probably happen if the regex itself has a
                // NUL, and that NUL re-occurs in the context presented by the
                // error message. In this case, just show as much as we can.
                let nul = err.nul_position();
                let msg = err.into_vec();
                CString::new(msg[0..nul].to_owned()).unwrap()
            }
        };
        let p = cmsg.as_ptr();
        err.message = Some(cmsg);
        p
    }
}
