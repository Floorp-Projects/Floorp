use std::ffi::{CStr, CString, NulError, FromBytesWithNulError};
use std::borrow::Cow;
use std::os::raw;

#[derive(Debug)]
pub struct NullError;

impl From<NulError> for NullError {
    fn from(_: NulError) -> NullError {
        NullError
    }
}

impl From<FromBytesWithNulError> for NullError {
    fn from(_: FromBytesWithNulError) -> NullError {
        NullError
    }
}

impl From<NullError> for ::std::io::Error {
    fn from(e: NullError) -> ::std::io::Error {
        ::std::io::Error::new(::std::io::ErrorKind::Other, format!("{}", e))
    }
}

impl ::std::error::Error for NullError {
    fn description(&self) -> &str { "non-final null byte found" }
}

impl ::std::fmt::Display for NullError {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "non-final null byte found")
    }
}

/// Checks for last byte and avoids allocating if it is zero.
///
/// Non-last null bytes still result in an error.
pub fn cstr_cow_from_bytes<'a>(slice: &'a [u8]) -> Result<Cow<'a, CStr>, NullError> {
    static ZERO: raw::c_char = 0;
    Ok(match slice.last() {
        // Slice out of 0 elements
        None => unsafe { Cow::Borrowed(CStr::from_ptr(&ZERO)) },
        // Slice with trailing 0
        Some(&0) => Cow::Borrowed(try!(CStr::from_bytes_with_nul(slice))),
        // Slice with no trailing 0
        Some(_) => Cow::Owned(try!(CString::new(slice))),
    })
}

#[inline]
pub fn ensure_compatible_types<T, E>() {
    #[cold]
    #[inline(never)]
    fn dopanic() {
        panic!("value of requested type cannot be dynamically loaded");
    }
    if ::std::mem::size_of::<T>() != ::std::mem::size_of::<E>() {
        dopanic()
    }
}
