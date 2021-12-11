use cstr::cstr;
use std::ffi::CStr;

const FOO: &CStr = cstr!(b"foo\xffbar");
static BAR: &CStr = cstr!("bar");

fn main() {
    assert_eq!(FOO, CStr::from_bytes_with_nul(b"foo\xffbar\0").unwrap());
    assert_eq!(BAR, CStr::from_bytes_with_nul(b"bar\0").unwrap());
}
