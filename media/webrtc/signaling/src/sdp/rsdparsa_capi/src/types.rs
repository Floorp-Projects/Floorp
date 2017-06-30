use libc::{size_t, uint32_t};

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
