use cstr::cstr;
use std::ffi::CStr;

macro_rules! cstr_expr {
    ($s:expr) => {
        cstr!($s)
    };
}

macro_rules! cstr_literal {
    ($s:literal) => {
        cstr!($s)
    };
}

fn main() {
    let foo: &'static CStr = cstr_expr!("foo");
    assert_eq!(foo, CStr::from_bytes_with_nul(b"foo\0").unwrap());
    let bar: &'static CStr = cstr_literal!("bar");
    assert_eq!(bar, CStr::from_bytes_with_nul(b"bar\0").unwrap());
}
