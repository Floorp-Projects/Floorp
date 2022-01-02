use cstr::cstr;
use std::ffi::CStr;

#[test]
#[deny(clippy::transmute_ptr_to_ref)]
fn deny_transmute_ptr_to_ref() {
    let s: &'static CStr = cstr!("foo\u{4e00}bar");
    let expected = b"foo\xe4\xb8\x80bar\0";
    assert_eq!(s, CStr::from_bytes_with_nul(expected).unwrap());
}
