mod sys;

#[cfg(feature = "logger")]
mod logger;

#[cfg(feature = "logger")]
pub use logger::OsLogger;

use crate::sys::*;
use std::ffi::{c_void, CString};

#[inline]
fn to_cstr(message: &str) -> CString {
    let fixed = message.replace('\0', "(null)");
    CString::new(fixed).unwrap()
}

#[repr(u8)]
pub enum Level {
    Debug = OS_LOG_TYPE_DEBUG,
    Info = OS_LOG_TYPE_INFO,
    Default = OS_LOG_TYPE_DEFAULT,
    Error = OS_LOG_TYPE_ERROR,
    Fault = OS_LOG_TYPE_FAULT,
}

#[cfg(feature = "logger")]
impl From<log::Level> for Level {
    fn from(other: log::Level) -> Self {
        match other {
            log::Level::Trace => Self::Debug,
            log::Level::Debug => Self::Info,
            log::Level::Info => Self::Default,
            log::Level::Warn => Self::Error,
            log::Level::Error => Self::Fault,
        }
    }
}

pub struct OsLog {
    inner: os_log_t,
}

unsafe impl Send for OsLog {}
unsafe impl Sync for OsLog {}

impl Drop for OsLog {
    fn drop(&mut self) {
        unsafe {
            if self.inner != wrapped_get_default_log() {
                os_release(self.inner as *mut c_void);
            }
        }
    }
}

impl OsLog {
    pub fn new(subsystem: &str, category: &str) -> Self {
        let subsystem = to_cstr(subsystem);
        let category = to_cstr(category);

        let inner = unsafe { os_log_create(subsystem.as_ptr(), category.as_ptr()) };

        assert!(!inner.is_null(), "Unexpected null value from os_log_create");

        Self { inner }
    }

    pub fn global() -> Self {
        let inner = unsafe { wrapped_get_default_log() };

        assert!(!inner.is_null(), "Unexpected null value for OS_DEFAULT_LOG");

        Self { inner }
    }

    pub fn with_level(&self, level: Level, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_with_type(self.inner, level as u8, message.as_ptr()) }
    }

    pub fn debug(&self, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_debug(self.inner, message.as_ptr()) }
    }

    pub fn info(&self, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_info(self.inner, message.as_ptr()) }
    }

    pub fn default(&self, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_default(self.inner, message.as_ptr()) }
    }

    pub fn error(&self, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_error(self.inner, message.as_ptr()) }
    }

    pub fn fault(&self, message: &str) {
        let message = to_cstr(message);
        unsafe { wrapped_os_log_fault(self.inner, message.as_ptr()) }
    }

    pub fn level_is_enabled(&self, level: Level) -> bool {
        unsafe { os_log_type_enabled(self.inner, level as u8) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_subsystem_interior_null() {
        let log = OsLog::new("com.example.oslog\0test", "category");
        log.with_level(Level::Debug, "Hi");
    }

    #[test]
    fn test_category_interior_null() {
        let log = OsLog::new("com.example.oslog", "category\0test");
        log.with_level(Level::Debug, "Hi");
    }

    #[test]
    fn test_message_interior_null() {
        let log = OsLog::new("com.example.oslog", "category");
        log.with_level(Level::Debug, "Hi\0test");
    }

    #[test]
    fn test_message_emoji() {
        let log = OsLog::new("com.example.oslog", "category");
        log.with_level(Level::Debug, "\u{1F601}");
    }

    #[test]
    fn test_global_log_with_level() {
        let log = OsLog::global();
        log.with_level(Level::Debug, "Debug");
        log.with_level(Level::Info, "Info");
        log.with_level(Level::Default, "Default");
        log.with_level(Level::Error, "Error");
        log.with_level(Level::Fault, "Fault");
    }

    #[test]
    fn test_global_log() {
        let log = OsLog::global();
        log.debug("Debug");
        log.info("Info");
        log.default("Default");
        log.error("Error");
        log.fault("Fault");
    }

    #[test]
    fn test_custom_log_with_level() {
        let log = OsLog::new("com.example.oslog", "testing");
        log.with_level(Level::Debug, "Debug");
        log.with_level(Level::Info, "Info");
        log.with_level(Level::Default, "Default");
        log.with_level(Level::Error, "Error");
        log.with_level(Level::Fault, "Fault");
    }

    #[test]
    fn test_custom_log() {
        let log = OsLog::new("com.example.oslog", "testing");
        log.debug("Debug");
        log.info("Info");
        log.default("Default");
        log.error("Error");
        log.fault("Fault");
    }
}
