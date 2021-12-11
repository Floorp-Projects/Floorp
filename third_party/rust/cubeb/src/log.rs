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
            let cstr = ::std::ffi::CString::new(format!("{}:{}: {}\n", file!(), line!(), $msg)).unwrap();
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
