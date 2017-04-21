use std::os::raw::c_char;

#[macro_export]
macro_rules! log_internal {
    ($level: expr, $msg: expr) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::g_cubeb_log_level {
                if let Some(log_callback) = $crate::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $msg, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!());
                }
            }
        }
    };
    ($level: expr, $fmt: expr, $($arg:tt)+) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level <= $crate::g_cubeb_log_level {
                if let Some(log_callback) = $crate::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $fmt, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!(), $($arg)+);
                }
            }
        }
    }
}

#[macro_export]
macro_rules! logv {
    ($msg: expr) => (log_internal!($crate::LogLevel::Verbose, $msg));
    ($fmt: expr, $($arg: tt)+) => (log_internal!($crate::LogLevel::Verbose, $fmt, $($arg)*));
}

#[macro_export]
macro_rules! log {
    ($msg: expr) => (log_internal!($crate::LogLevel::Normal, $msg));
    ($fmt: expr, $($arg: tt)+) => (log_internal!($crate::LogLevel::Normal, $fmt, $($arg)*));
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum LogLevel {
    Disabled = 0,
    Normal = 1,
    Verbose = 2,
}

pub type LogCallback = Option<unsafe extern "C" fn(fmt: *const c_char, ...)>;

extern "C" {
    pub static g_cubeb_log_level: LogLevel;
    pub static g_cubeb_log_callback: LogCallback;
}

#[test]
fn test_normal_logging() {
    log!("This is log at normal level");
    log!("Formatted log %d", 1);
}

#[test]
fn test_verbose_logging() {
    logv!("This is a log at verbose level");
    logv!("Formatted log %d", 1);
}
