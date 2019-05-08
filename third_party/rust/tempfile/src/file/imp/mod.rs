#[cfg(any(unix, target_os = "redox"))]
mod unix;

#[cfg(any(unix, target_os = "redox"))]
pub use self::unix::*;

#[cfg(windows)]
mod windows;

#[cfg(windows)]
pub use self::windows::*;
