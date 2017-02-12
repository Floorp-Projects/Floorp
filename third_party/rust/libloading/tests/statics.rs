extern crate libloading;
use libloading::{Symbol, Library};

const LIBPATH: &'static str = concat!(env!("OUT_DIR"), "/libtest_helpers.dll");

#[test]
fn test_static_u32() {
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
