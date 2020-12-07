use cstr::cstr;
use std::ffi::CStr;

fn main() {
    let foo: &'static CStr = cstr!(foobar);
    assert_eq!(foo, CStr::from_bytes_with_nul(b"foobar\0").unwrap());
    let foo: &'static CStr = cstr!(r#foobar);
    assert_eq!(foo, CStr::from_bytes_with_nul(b"r#foobar\0").unwrap());
}
