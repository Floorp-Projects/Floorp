#![cfg(unix)]

use std::env;
use std::path::PathBuf;
use std::mem;
use std::ptr;
use std::ffi::CStr;
use std::ffi::OsString;
use std::os::unix::ffi::OsStringExt;

extern crate libc;

// https://github.com/rust-lang/rust/blob/master/src/libstd/sys/unix/os.rs#L498
pub fn home_dir() -> Option<PathBuf> {
    return env::var_os("HOME").and_then(|h| { if h.is_empty() { None } else { Some(h) } } ).or_else(|| unsafe {
        fallback()
    }).map(PathBuf::from);

    #[cfg(any(target_os = "android",
              target_os = "ios",
              target_os = "emscripten"))]
    unsafe fn fallback() -> Option<OsString> { None }
    #[cfg(not(any(target_os = "android",
                  target_os = "ios",
                  target_os = "emscripten")))]
    unsafe fn fallback() -> Option<OsString> {
        let amt = match libc::sysconf(libc::_SC_GETPW_R_SIZE_MAX) {
            n if n < 0 => 512 as usize,
            n => n as usize,
        };
        let mut buf = Vec::with_capacity(amt);
        let mut passwd: libc::passwd = mem::zeroed();
        let mut result = ptr::null_mut();
        match libc::getpwuid_r(libc::getuid(), &mut passwd, buf.as_mut_ptr(),
                               buf.capacity(), &mut result) {
            0 if !result.is_null() => {
                let ptr = passwd.pw_dir as *const _;
                let bytes = CStr::from_ptr(ptr).to_bytes();
                if bytes.is_empty() {
                    None
                } else {
                    Some(OsStringExt::from_vec(bytes.to_vec()))
                }
            },
            _ => None,
        }
    }
}

