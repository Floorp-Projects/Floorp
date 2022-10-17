// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::os::raw::c_char;

/// Maximum length in bytes for a log message.
/// Longer messages are silently truncated.  See `write_str`.
const LOG_LIMIT: usize = 1024;

struct StaticCString<const N: usize> {
    buf: [std::mem::MaybeUninit<u8>; N],
    len: usize,
}

impl<const N: usize> StaticCString<N> {
    fn new() -> Self {
        StaticCString {
            buf: unsafe { std::mem::MaybeUninit::uninit().assume_init() },
            len: 0,
        }
    }

    fn as_cstr(&self) -> &std::ffi::CStr {
        unsafe {
            std::ffi::CStr::from_bytes_with_nul_unchecked(std::slice::from_raw_parts(
                self.buf.as_ptr().cast::<u8>(),
                self.len,
            ))
        }
    }
}

impl<const N: usize> std::fmt::Write for StaticCString<N> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        use std::convert::TryInto;
        let s = s.as_bytes();
        let end = s.len().min(N.checked_sub(1).unwrap() - self.len);
        debug_assert_eq!(s.len(), end, "message truncated");
        unsafe {
            std::ptr::copy_nonoverlapping(
                s[..end].as_ptr(),
                self.buf
                    .as_mut_ptr()
                    .cast::<u8>()
                    .offset(self.len.try_into().unwrap()),
                end,
            )
        };
        self.len += end;
        self.buf[self.len].write(0);
        Ok(())
    }
}

/// Formats `$file:line: $msg\n` into an on-stack buffer of size `LOG_LIMIT`,
/// then calls `log_callback` with a pointer to the formatted message.
pub fn cubeb_log_internal_buf_fmt(
    log_callback: unsafe extern "C" fn(*const c_char, ...),
    file: &str,
    line: u32,
    msg: std::fmt::Arguments,
) {
    let filename = std::path::Path::new(file)
        .file_name()
        .unwrap()
        .to_str()
        .unwrap();
    let mut buf = StaticCString::<LOG_LIMIT>::new();
    let _ = std::fmt::write(&mut buf, format_args!("{}:{}: {}\n", filename, line, msg));
    unsafe {
        log_callback(buf.as_cstr().as_ptr());
    };
}

#[macro_export]
macro_rules! cubeb_log_internal {
    ($log_callback: expr, $level: expr, $fmt: expr, $($arg: expr),+) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::ffi::cubeb_log_get_level().into() {
                if let Some(log_callback) = $log_callback {
                    $crate::log::cubeb_log_internal_buf_fmt(log_callback, file!(), line!(), format_args!($fmt, $($arg),+));
                }
            }
        }
    };
    ($log_callback: expr, $level: expr, $msg: expr) => {
        cubeb_log_internal!($log_callback, $level, "{}", $msg);
    };
}

#[macro_export]
macro_rules! cubeb_log {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::cubeb_log_get_callback(), $crate::LogLevel::Normal, $($arg),+));
}

#[macro_export]
macro_rules! cubeb_logv {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::cubeb_log_get_callback(), $crate::LogLevel::Verbose, $($arg),+));
}

#[macro_export]
macro_rules! cubeb_alog {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::cubeb_async_log.into(), $crate::LogLevel::Normal, $($arg),+));
}

#[macro_export]
macro_rules! cubeb_alogv {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::cubeb_async_log.into(), $crate::LogLevel::Verbose, $($arg),+));
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_normal_logging_sync() {
        cubeb_log!("This is synchronous log output at normal level");
        cubeb_log!("{} Formatted log", 1);
        cubeb_log!("{} Formatted {} log {}", 1, 2, 3);
    }

    #[test]
    fn test_verbose_logging_sync() {
        cubeb_logv!("This is synchronous log output at verbose level");
        cubeb_logv!("{} Formatted log", 1);
        cubeb_logv!("{} Formatted {} log {}", 1, 2, 3);
    }

    #[test]
    fn test_normal_logging_async() {
        cubeb_alog!("This is asynchronous log output at normal level");
        cubeb_alog!("{} Formatted log", 1);
        cubeb_alog!("{} Formatted {} log {}", 1, 2, 3);
    }

    #[test]
    fn test_verbose_logging_async() {
        cubeb_alogv!("This is asynchronous log output at verbose level");
        cubeb_alogv!("{} Formatted log", 1);
        cubeb_alogv!("{} Formatted {} log {}", 1, 2, 3);
    }
}
