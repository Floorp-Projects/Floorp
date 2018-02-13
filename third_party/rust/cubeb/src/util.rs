// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use cubeb_core::Error;
use ffi;
use std::ffi::CString;

/// A class of types that can be converted to C strings.
///
/// These types are represented internally as byte slices and it is quite rare
/// for them to contain an interior 0 byte.
pub trait IntoCString {
    /// Consume this container, converting it into a CString
    fn into_c_string(self) -> Result<CString, Error>;
}

impl<'a, T: IntoCString + Clone> IntoCString for &'a T {
    fn into_c_string(self) -> Result<CString, Error> { self.clone().into_c_string() }
}

impl<'a> IntoCString for &'a str {
    fn into_c_string(self) -> Result<CString, Error> {
        match CString::new(self) {
            Ok(s) => Ok(s),
            Err(_) => Err(unsafe { Error::from_raw(ffi::CUBEB_ERROR) }),
        }
    }
}

impl IntoCString for String {
    fn into_c_string(self) -> Result<CString, Error> {
        match CString::new(self.into_bytes()) {
            Ok(s) => Ok(s),
            Err(_) => Err(unsafe { Error::from_raw(ffi::CUBEB_ERROR) }),
        }
    }
}

impl IntoCString for CString {
    fn into_c_string(self) -> Result<CString, Error> { Ok(self) }
}

impl IntoCString for Vec<u8> {
    fn into_c_string(self) -> Result<CString, Error> { Ok(try!(CString::new(self))) }
}
