use cstr::cstr;
use std::ffi::CStr;

fn main() {
    let foo: &'static CStr = cstr!(b"foo\xffbar");
    assert_eq!(foo, CStr::from_bytes_with_nul(b"foo\xffbar\0").unwrap());
}
