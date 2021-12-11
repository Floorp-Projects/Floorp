// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#[macro_export]
macro_rules! cubeb_log_internal {
    ($level: expr, $msg: expr) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::ffi::g_cubeb_log_level.into() {
                cubeb_log_internal!(__INTERNAL__ $msg);
            }
        }
    };
    ($level: expr, $fmt: expr, $($arg: expr),+) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::ffi::g_cubeb_log_level.into() {
                cubeb_log_internal!(__INTERNAL__ format!($fmt, $($arg),*));
            }
        }
    };
    (__INTERNAL__ $msg: expr) => {
        if let Some(log_callback) = $crate::ffi::g_cubeb_log_callback {
            use std::io::Write;

            let mut buf = [0 as u8; 1024];
            let filename = std::path::Path::new(file!())
                .file_name()
                .unwrap()
                .to_str()
                .unwrap();
            // 2 for ':', 1 for ' ', 1 for '\n', and 1 for converting `line!()` to number of digits
            let len = filename.len() + ((line!() as f32).log10().trunc() as usize) + $msg.len() + 5;
            debug_assert!(len < buf.len(), "log will be truncated");
            let _ = write!(&mut buf[..], "{}:{}: {}\n", filename, line!(), $msg);
            let last = std::cmp::min(len, buf.len() - 1);
            buf[last] = 0;
            let cstr = unsafe { std::ffi::CStr::from_bytes_with_nul_unchecked(&buf[..=last]) };
            log_callback(cstr.as_ptr());
        }
    }
}

#[macro_export]
macro_rules! cubeb_logv {
    ($msg: expr) => (cubeb_log_internal!($crate::LogLevel::Verbose, $msg));
    ($fmt: expr, $($arg: expr),+) => (cubeb_log_internal!($crate::LogLevel::Verbose, $fmt, $($arg),*));
}

#[macro_export]
macro_rules! cubeb_log {
    ($msg: expr) => (cubeb_log_internal!($crate::LogLevel::Normal, $msg));
    ($fmt: expr, $($arg: expr),+) => (cubeb_log_internal!($crate::LogLevel::Normal, $fmt, $($arg),*));
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_normal_logging() {
        cubeb_log!("This is log at normal level");
        cubeb_log!("{} Formatted log", 1);
        cubeb_log!("{} Formatted {} log {}", 1, 2, 3);
    }

    #[test]
    fn test_verbose_logging() {
        cubeb_logv!("This is a log at verbose level");
        cubeb_logv!("{} Formatted log", 1);
        cubeb_logv!("{} Formatted {} log {}", 1, 2, 3);
    }
}
