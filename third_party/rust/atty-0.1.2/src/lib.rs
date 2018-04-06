//! atty is a simple utility that answers one question
//! > is this a tty?
//!
//! usage is just as simple
//!
//! ```
//! if atty::is() {
//!   println!("i'm a tty")
//! }
//! ```
//!
//! ```
//! if atty::isnt() {
//!   println!("i'm not a tty")
//! }
//! ```

extern crate libc;

/// returns true if this is a tty
#[cfg(unix)]
pub fn is() -> bool {
    let r = unsafe { libc::isatty(libc::STDOUT_FILENO) };
    r != 0
}

/// returns true if this is a tty
#[cfg(windows)]
pub fn is() -> bool {
    extern crate kernel32;
    extern crate winapi;
    use std::ptr;
    let handle: winapi::HANDLE = unsafe {
        kernel32::CreateFileA(b"CONOUT$\0".as_ptr() as *const i8,
                              winapi::GENERIC_READ | winapi::GENERIC_WRITE,
                              winapi::FILE_SHARE_WRITE,
                              ptr::null_mut(),
                              winapi::OPEN_EXISTING,
                              0,
                              ptr::null_mut())
    };
    if handle == winapi::INVALID_HANDLE_VALUE {
        return false;
    }
    let mut out = 0;
    unsafe { kernel32::GetConsoleMode(handle, &mut out) != 0 }
}

/// returns true if this is _not_ a tty
pub fn isnt() -> bool {
    !is()
}

#[cfg(test)]
mod tests {
    use super::is;

    #[test]
    fn is_test() {
        assert!(is())
    }
}
