extern crate libloading;
use libloading::{Symbol, Library};

const LIBPATH: &'static str = concat!(env!("OUT_DIR"), "/libtest_helpers.dll");

fn make_helpers() {
    static ONCE: ::std::sync::Once = ::std::sync::ONCE_INIT;
    ONCE.call_once(|| {
        let mut outpath = String::from(if let Some(od) = option_env!("OUT_DIR") { od } else { return });
        let rustc = option_env!("RUSTC").unwrap_or_else(|| { "rustc".into() });
        outpath.push_str(&"/libtest_helpers.dll"); // extension for windows required, POSIX does not care.
        let _ = ::std::process::Command::new(rustc)
            .arg("src/test_helpers.rs")
            .arg("-o")
            .arg(outpath)
            .arg("-O")
            .output()
            .expect("could not compile the test helpers!");
    });
}

#[test]
fn test_id_u32() {
    make_helpers();
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
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let f: Symbol<unsafe extern fn(S) -> S> = lib.get(b"test_identity_struct\0").unwrap();
        assert_eq!(S { a: 1, b: 2, c: 3, d: 4 }, f(S { a: 1, b: 2, c: 3, d: 4 }));
    }
}

#[test]
fn test_0_no_0() {
    make_helpers();
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
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        lib.get::<*mut ()>(b"test_does_not_exist").err().unwrap();
        lib.get::<*mut ()>(b"test_does_not_exist\0").err().unwrap();
    }
}

#[test]
fn interior_null_fails() {
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        lib.get::<*mut ()>(b"test_does\0_not_exist").err().unwrap();
        lib.get::<*mut ()>(b"test\0_does_not_exist\0").err().unwrap();
    }
}

#[test]
#[should_panic]
fn test_incompatible_type() {
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let _ = lib.get::<()>(b"test_identity_u32\0");
    }
}

#[test]
#[should_panic]
fn test_incompatible_type_named_fn() {
    make_helpers();
    unsafe fn get<'a, T>(l: &'a Library, _: T) -> libloading::Result<Symbol<'a, T>> {
        l.get::<T>(b"test_identity_u32\0")
    }
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let _ = get(&lib, test_incompatible_type_named_fn);
    }
}

#[test]
fn test_static_u32() {
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let var: Symbol<*mut u32> = lib.get(b"TEST_STATIC_U32\0").unwrap();
        **var = 42;
        let help: Symbol<unsafe extern fn() -> u32> = lib.get(b"test_get_static_u32\0").unwrap();
        assert_eq!(42, help());
    }
}

#[test]
fn test_static_ptr() {
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let var: Symbol<*mut *mut ()> = lib.get(b"TEST_STATIC_PTR\0").unwrap();
        **var = *var as *mut _;
        let works: Symbol<unsafe extern fn() -> bool> =
            lib.get(b"test_check_static_ptr\0").unwrap();
        assert!(works());
    }
}

#[cfg(any(windows, target_os="linux"))]
#[cfg(test_nightly)]
#[test]
fn test_tls_static() {
    make_helpers();
    let lib = Library::new(LIBPATH).unwrap();
    unsafe {
        let var: Symbol<*mut u32> = lib.get(b"TEST_THREAD_LOCAL\0").unwrap();
        **var = 84;
        let help: Symbol<unsafe extern fn() -> u32> = lib.get(b"test_get_thread_local\0").unwrap();
        assert_eq!(84, help());
    }
    ::std::thread::spawn(move || unsafe {
        let help: Symbol<unsafe extern fn() -> u32> = lib.get(b"test_get_thread_local\0").unwrap();
        assert_eq!(0, help());
    }).join().unwrap();
}
