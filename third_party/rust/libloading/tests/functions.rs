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
        lib.get::<*mut ()>(b"test_does_not_exist").err().unwrap();
        lib.get::<*mut ()>(b"test_does_not_exist\0").err().unwrap();
    }
}

#[test]
fn interior_null_fails() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        lib.get::<*mut ()>(b"test_does\0_not_exist").err().unwrap();
        lib.get::<*mut ()>(b"test\0_does_not_exist\0").err().unwrap();
    }
}

#[test]
#[should_panic]
fn test_incompatible_type() {
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let _ = lib.get::<()>(b"test_identity_u32\0");
    }
}

#[test]
#[should_panic]
fn test_incompatible_type_named_fn() {
    unsafe fn get<'a, T>(l: &'a Library, _: T) -> libloading::Result<Symbol<'a, T>> {
        l.get::<T>(b"test_identity_u32\0")
    }
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let _ = get(&lib, test_incompatible_type_named_fn);
    }
}
