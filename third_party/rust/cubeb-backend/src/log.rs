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
                if let Some(log_callback) = $crate::ffi::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $msg, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!());
                }
            }
        }
    };
    ($level: expr, $fmt: expr, $($arg:tt)+) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::ffi::g_cubeb_log_level.into() {
                if let Some(log_callback) = $crate::ffi::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $fmt, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!(), $($arg)+);
                }
            }
        }
    }
}

#[macro_export]
macro_rules! cubeb_logv {
    ($msg: expr) => (cubeb_log_internal!($crate::LogLevel::Verbose, $msg));
    ($fmt: expr, $($arg: tt)+) => (cubeb_log_internal!($crate::LogLevel::Verbose, $fmt, $($arg)*));
}

#[macro_export]
macro_rules! cubeb_log {
    ($msg: expr) => (cubeb_log_internal!($crate::LogLevel::Normal, $msg));
    ($fmt: expr, $($arg: tt)+) => (cubeb_log_internal!($crate::LogLevel::Normal, $fmt, $($arg)*));
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_normal_logging() {
        cubeb_log!("This is log at normal level");
        cubeb_log!("Formatted log %d", 1);
    }

    #[test]
    fn test_verbose_logging() {
        cubeb_logv!("This is a log at verbose level");
        cubeb_logv!("Formatted log %d", 1);
    }
}
