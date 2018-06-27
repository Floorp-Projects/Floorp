use libc::{size_t, uint8_t, uint16_t, uint32_t};
use std::ffi::CStr;
use std::{str, slice};
use std::error::Error;
use std::boxed::Box;

use nserror::{nsresult, NS_OK, NS_ERROR_INVALID_ARG};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct StringView {
    buffer: *const u8,
    len: size_t
}

pub const NULL_STRING: StringView = StringView { buffer: 0 as *const u8,
                                                 len: 0 };

impl<'a> From<&'a str> for StringView {
    fn from(input: &str) -> StringView {
        StringView { buffer: input.as_ptr(), len: input.len()}
    }
}

impl Into<Result<String,Box<Error>>> for StringView {
    fn into(self) -> Result<String,Box<Error>> {

        // This block must be unsafe as it converts a StringView, most likly provided from the
        // C++ code, into a rust String and thus needs to operate with raw pointers.
        let string_slice: &[u8];
        unsafe {
            // Add one to the length as the length passed in the StringView is the length of
            // the string and is missing the null terminator
            string_slice = slice::from_raw_parts(self.buffer, self.len+1 as usize);
        }

         let c_str = match CStr::from_bytes_with_nul(string_slice) {
                 Ok(string) => string,
                 Err(x) => { return Err(Box::new(x)); },
         };

         let str_slice: &str = match str::from_utf8(c_str.to_bytes()) {
                 Ok(string) => string,
                 Err(x) => { return Err(Box::new(x)); },
         };

         Ok(str_slice.to_string())
    }
}

impl<'a, T: AsRef<str>> From<&'a Option<T>> for StringView {
    fn from(input: &Option<T>) -> StringView {
        match *input {
            Some(ref x) => StringView { buffer: x.as_ref().as_ptr(),
                                        len: x.as_ref().len()},
            None => NULL_STRING
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn string_vec_len(vec: *const Vec<String>) -> size_t {
    (*vec).len() as size_t
}

#[no_mangle]
pub unsafe extern "C" fn string_vec_get_view(vec: *const Vec<String>,
                                             index: size_t,
                                             ret: *mut StringView) -> nsresult {
    match (*vec).get(index) {
        Some(ref string) => {
            *ret = StringView::from(string.as_str());
            NS_OK
        },
        None => NS_ERROR_INVALID_ARG
    }
}

#[no_mangle]
pub unsafe extern "C" fn u32_vec_len(vec: *const Vec<u32>) -> size_t {
    (*vec).len()
}

#[no_mangle]
pub unsafe extern "C" fn u32_vec_get(vec: *const Vec<u32>,
                                     index: size_t,
                                     ret: *mut uint32_t) -> nsresult {
    match (*vec).get(index) {
        Some(val) => {
            *ret = *val;
            NS_OK
        },
        None => NS_ERROR_INVALID_ARG
    }
}

#[no_mangle]
pub unsafe extern "C" fn u16_vec_len(vec: *const Vec<u16>) -> size_t {
    (*vec).len()
}

#[no_mangle]
pub unsafe extern "C" fn u16_vec_get(vec: *const Vec<u16>,
                                    index: size_t,
                                    ret: *mut uint16_t) -> nsresult {
    match (*vec).get(index) {
        Some(val) => {
            *ret = *val;
            NS_OK
        },
        None => NS_ERROR_INVALID_ARG
    }
}

#[no_mangle]
pub unsafe extern "C" fn u8_vec_len(vec: *const Vec<u8>) -> size_t {
    (*vec).len()
}

#[no_mangle]
pub unsafe extern "C" fn u8_vec_get(vec: *const Vec<u8>,
                                    index: size_t,
                                    ret: *mut uint8_t) -> nsresult {
    match (*vec).get(index) {
        Some(val) => {
            *ret = *val;
            NS_OK
        },
        None => NS_ERROR_INVALID_ARG
    }
}
