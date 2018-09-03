use std::{io, mem};

use super::cvt;
use super::libc::{c_ushort, ioctl, STDOUT_FILENO};

#[repr(C)]
struct TermSize {
    row: c_ushort,
    col: c_ushort,
    _x: c_ushort,
    _y: c_ushort,
}

#[cfg(target_os = "linux")]
pub const TIOCGWINSZ: usize = 0x00005413;

#[cfg(not(target_os = "linux"))]
pub const TIOCGWINSZ: usize = 0x40087468;

// Since attributes on non-item statements is not stable yet, we use a function.
#[cfg(not(target_os = "android"))]
#[cfg(not(target_os = "redox"))]
#[cfg(target_pointer_width = "64")]
#[cfg(not(target_env = "musl"))]
fn tiocgwinsz() -> u64 {
    TIOCGWINSZ as u64
}
#[cfg(not(target_os = "android"))]
#[cfg(not(target_os = "redox"))]
#[cfg(target_pointer_width = "32")]
#[cfg(not(target_env = "musl"))]
fn tiocgwinsz() -> u32 {
    TIOCGWINSZ as u32
}

#[cfg(any(target_env = "musl", target_os = "android"))]
fn tiocgwinsz() -> i32 {
    TIOCGWINSZ as i32
}

/// Get the size of the terminal.
pub fn terminal_size() -> io::Result<(u16, u16)> {
    unsafe {
        let mut size: TermSize = mem::zeroed();
        cvt(ioctl(STDOUT_FILENO, tiocgwinsz(), &mut size as *mut _))?;
        Ok((size.col as u16, size.row as u16))
    }
}
