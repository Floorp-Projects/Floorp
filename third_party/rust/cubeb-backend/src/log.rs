// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

/// Annotates input buffer string with logging information.
/// Returns result as a ffi::CStr for use with native cubeb logging functions.
pub fn cubeb_log_internal_buf_fmt<'a>(
    buf: &'a mut [u8; 1024],
    file: &str,
    line: u32,
    msg: &str,
) -> &'a std::ffi::CStr {
    use std::io::Write;
    let filename = std::path::Path::new(file)
        .file_name()
        .unwrap()
        .to_str()
        .unwrap();
    // 2 for ':', 1 for ' ', 1 for '\n', and 1 for converting `line!()` to number of digits
    let len = filename.len() + ((line as f32).log10().trunc() as usize) + msg.len() + 5;
    debug_assert!(len < buf.len(), "log will be truncated");
    let _ = writeln!(&mut buf[..], "{}:{}: {}", filename, line, msg);
    let last = std::cmp::min(len, buf.len() - 1);
    buf[last] = 0;
    let cstr = unsafe { std::ffi::CStr::from_bytes_with_nul_unchecked(&buf[..=last]) };
    cstr
}

#[macro_export]
macro_rules! cubeb_log_internal {
    ($log_callback: expr, $level: expr, $fmt: expr, $($arg: expr),+) => {
        cubeb_log_internal!($log_callback, $level, format!($fmt, $($arg),*));
    };
    ($log_callback: expr, $level: expr, $msg: expr) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::ffi::g_cubeb_log_level.into() {
                if let Some(log_callback) = $log_callback {
                    let mut buf = [0u8; 1024];
                    log_callback(
                        $crate::log::cubeb_log_internal_buf_fmt(&mut buf, file!(), line!(), &$msg)
                            .as_ptr(),
                    );
                }
            }
        }
    };
}

#[macro_export]
macro_rules! cubeb_log {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::g_cubeb_log_callback, $crate::LogLevel::Normal, $($arg),+));
}

#[macro_export]
macro_rules! cubeb_logv {
    ($($arg: expr),+) => (cubeb_log_internal!($crate::ffi::g_cubeb_log_callback, $crate::LogLevel::Verbose, $($arg),+));
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
