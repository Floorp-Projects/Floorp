#![macro_use]

use libc::{c_void, c_int, c_char, free};
use std::{fmt, str};
use std::ffi::CStr;
use std::error::Error as StdError;

/// ALSA error
///
/// Most ALSA functions can return a negative error code.
/// If so, then that error code is wrapped into this `Error` struct.
/// An Error is also returned in case ALSA returns a string that
/// cannot be translated into Rust's UTF-8 strings.
#[derive(Debug, Clone, PartialEq, Copy)]
pub struct Error(&'static str, nix::Error);

pub type Result<T> = ::std::result::Result<T, Error>;

macro_rules! acheck {
    ($f: ident ( $($x: expr),* ) ) => {{
        let r = unsafe { alsa::$f( $($x),* ) };
        if r < 0 { Err(Error::new(stringify!($f), -r as ::libc::c_int)) }
        else { Ok(r) }
    }}
}

pub fn from_const<'a>(func: &'static str, s: *const c_char) -> Result<&'a str> {
    if s.is_null() { return Err(invalid_str(func)) };
    let cc = unsafe { CStr::from_ptr(s) };
    str::from_utf8(cc.to_bytes()).map_err(|_| invalid_str(func))
}

pub fn from_alloc(func: &'static str, s: *mut c_char) -> Result<String> {
    if s.is_null() { return Err(invalid_str(func)) };
    let c = unsafe { CStr::from_ptr(s) };
    let ss = str::from_utf8(c.to_bytes()).map_err(|_| {
        unsafe { free(s as *mut c_void); }
        invalid_str(func)
    })?.to_string();
    unsafe { free(s as *mut c_void); }
    Ok(ss)
}

pub fn from_code(func: &'static str, r: c_int) -> Result<c_int> {
    if r < 0 { Err(Error::new(func, r)) }
    else { Ok(r) }
}

impl Error {
    pub fn new(func: &'static str, res: c_int) -> Error {
        let errno = nix::errno::Errno::from_i32(res as i32);
        Error(func, errno)
    }

    pub fn unsupported(func: &'static str) -> Error {
        Error(func, nix::Error::ENOTSUP)
    }

    /// The function which failed.
    pub fn func(&self) -> &'static str { self.0 }


    /// Underlying error
    ///
    /// Match this against the re-export of `nix::Error` in this crate, not against a specific version
    /// of the nix crate. The nix crate version might be updated with minor updates of this library.
    pub fn errno(&self) -> nix::Error { self.1 }

    /// Underlying error
    ///
    /// Match this against the re-export of `nix::Error` in this crate, not against a specific version
    /// of the nix crate. The nix crate version might be updated with minor updates of this library.
    pub fn nix_error(&self) -> nix::Error { self.1 }
}

pub fn invalid_str(func: &'static str) -> Error { Error(func, nix::Error::EILSEQ) }

impl StdError for Error {
    fn source(&self) -> Option<&(dyn StdError + 'static)> { Some(&self.1) }
    fn description(&self) -> &str { "ALSA error" }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "ALSA function '{}' failed with error '{}'", self.0, self.1)
    }
}

impl From<Error> for fmt::Error {
    fn from(_: Error) -> fmt::Error { fmt::Error }
}


#[test]
fn broken_pcm_name() {
    use std::ffi::CString;
    let e = crate::PCM::open(&*CString::new("this_PCM_does_not_exist").unwrap(), crate::Direction::Playback, false).err().unwrap();
    assert_eq!(e.func(), "snd_pcm_open");
    assert_eq!(e.errno(), nix::errno::Errno::ENOENT);
}
