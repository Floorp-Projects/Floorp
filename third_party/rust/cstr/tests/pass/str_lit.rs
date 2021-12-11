use cstr::cstr;
use std::ffi::CStr;

fn main() {
    let foo: &'static CStr = cstr!("foo\u{4e00}bar");
    let expected = b"foo\xe4\xb8\x80bar\0";
    assert_eq!(foo, CStr::from_bytes_with_nul(expected).unwrap());
}
