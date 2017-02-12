extern crate libloading;
use libloading::{Symbol, Library};

const LIBPATH: &'static str = concat!(env!("OUT_DIR"), "/libtest_helpers.dll");

#[test]
fn test_id_u32() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let f: Symbol<unsafe extern fn(u32) -> u32> = lib.get(b"test_identity_u32\0").unwrap();
        assert_eq!(42, f(42));
    }
}

#[repr(C)]
#[derive(Clone,Copy,PartialEq,Debug)]
struct S {
    a: u64,
    b: u32,
    c: u16,
    d: u8
}

#[test]
fn test_id_struct() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let f: Symbol<unsafe extern fn(S) -> S> = lib.get(b"test_identity_struct\0").unwrap();
        assert_eq!(S { a: 1, b: 2, c: 3, d: 4 }, f(S { a: 1, b: 2, c: 3, d: 4 }));
    }
}

#[test]
fn test_0_no_0() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let f: Symbol<unsafe extern fn(S) -> S> = lib.get(b"test_identity_struct\0").unwrap();
        let f2: Symbol<unsafe extern fn(S) -> S> = lib.get(b"test_identity_struct").unwrap();
        assert_eq!(*f, *f2);
    }
}

#[test]
fn wrong_name_fails() {
    Library::new(concat!(env!("OUT_DIR"), "/libtest_help")).err().unwrap();
}

#[test]
fn missing_symbol_fails() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        lib.get::<Symbol<()>>(b"test_does_not_exist").err().unwrap();
        lib.get::<Symbol<()>>(b"test_does_not_exist\0").err().unwrap();
    }
}

#[test]
fn interior_null_fails() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        lib.get::<Symbol<()>>(b"test_does\0_not_exist").err().unwrap();
        lib.get::<Symbol<()>>(b"test\0_does_not_exist\0").err().unwrap();
    }
}
