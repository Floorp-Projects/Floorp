#![allow(non_snake_case)]

extern crate nsstring;

use nsstring::*;
use std::ffi::CString;
use std::fmt::Write;
use std::os::raw::c_char;

fn nonfatal_fail(msg: String) {
    extern "C" {
        fn GTest_ExpectFailure(message: *const c_char);
    }
    unsafe {
        let msg = CString::new(msg).unwrap();
        GTest_ExpectFailure(msg.as_ptr());
    }
}

/// This macro checks if the two arguments are equal, and causes a non-fatal
/// GTest test failure if they are not.
macro_rules! expect_eq {
    ($x:expr, $y:expr) => {
        match (&$x, &$y) {
            (x, y) => {
                if *x != *y {
                    nonfatal_fail(format!(
                        "check failed: (`{:?}` == `{:?}`) at {}:{}",
                        x,
                        y,
                        file!(),
                        line!()
                    ))
                }
            }
        }
    };
}

#[no_mangle]
pub extern "C" fn Rust_StringFromCpp(cs: *const nsACString, s: *const nsAString) {
    unsafe {
        expect_eq!(&*cs, "Hello, World!");
        expect_eq!(&*s, "Hello, World!");
    }
}

#[no_mangle]
pub extern "C" fn Rust_AssignFromRust(cs: *mut nsACString, s: *mut nsAString) {
    unsafe {
        (*cs).assign(&nsCString::from("Hello, World!"));
        expect_eq!(&*cs, "Hello, World!");
        (*s).assign(&nsString::from("Hello, World!"));
        expect_eq!(&*s, "Hello, World!");
    }
}

extern "C" {
    fn Cpp_AssignFromCpp(cs: *mut nsACString, s: *mut nsAString);
}

#[no_mangle]
pub extern "C" fn Rust_AssignFromCpp() {
    let mut cs = nsCString::new();
    let mut s = nsString::new();
    unsafe {
        Cpp_AssignFromCpp(&mut *cs, &mut *s);
    }
    expect_eq!(cs, "Hello, World!");
    expect_eq!(s, "Hello, World!");
}

#[no_mangle]
pub extern "C" fn Rust_StringWrite() {
    let mut cs = nsCString::new();
    let mut s = nsString::new();

    write!(s, "a").unwrap();
    write!(cs, "a").unwrap();
    expect_eq!(s, "a");
    expect_eq!(cs, "a");
    write!(s, "bc").unwrap();
    write!(cs, "bc").unwrap();
    expect_eq!(s, "abc");
    expect_eq!(cs, "abc");
    write!(s, "{}", 123).unwrap();
    write!(cs, "{}", 123).unwrap();
    expect_eq!(s, "abc123");
    expect_eq!(cs, "abc123");
}

#[no_mangle]
pub extern "C" fn Rust_FromEmptyRustString() {
    let mut test = nsString::from("Blah");
    test.assign_utf8(&nsCString::from(String::new()));
    assert!(test.is_empty());
}

#[no_mangle]
pub extern "C" fn Rust_WriteToBufferFromRust(
    cs: *mut nsACString,
    s: *mut nsAString,
    fallible_cs: *mut nsACString,
    fallible_s: *mut nsAString,
) {
    unsafe {
        let cs_buf = (*cs).to_mut();
        let s_buf = (*s).to_mut();
        let fallible_cs_buf = (*fallible_cs).fallible_to_mut().unwrap();
        let fallible_s_buf = (*fallible_s).fallible_to_mut().unwrap();

        cs_buf[0] = b'A';
        cs_buf[1] = b'B';
        cs_buf[2] = b'C';
        s_buf[0] = b'A' as u16;
        s_buf[1] = b'B' as u16;
        s_buf[2] = b'C' as u16;
        fallible_cs_buf[0] = b'A';
        fallible_cs_buf[1] = b'B';
        fallible_cs_buf[2] = b'C';
        fallible_s_buf[0] = b'A' as u16;
        fallible_s_buf[1] = b'B' as u16;
        fallible_s_buf[2] = b'C' as u16;
    }
}

#[no_mangle]
pub extern "C" fn Rust_VoidStringFromRust(cs: &mut nsACString, s: &mut nsAString) {
    cs.set_is_void(true);
    s.set_is_void(true);
}
