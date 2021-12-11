#![allow(non_camel_case_types)]
#![allow(dead_code)]

use std::{ffi::c_void, os::raw::c_char};

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct os_log_s {
    _unused: [u8; 0],
}

pub type os_log_t = *mut os_log_s;
pub type os_log_type_t = u8;

pub const OS_LOG_TYPE_DEFAULT: os_log_type_t = 0;
pub const OS_LOG_TYPE_INFO: os_log_type_t = 1;
pub const OS_LOG_TYPE_DEBUG: os_log_type_t = 2;
pub const OS_LOG_TYPE_ERROR: os_log_type_t = 16;
pub const OS_LOG_TYPE_FAULT: os_log_type_t = 17;

/// Provided by the OS.
extern "C" {
    pub fn os_log_create(subsystem: *const c_char, category: *const c_char) -> os_log_t;
    pub fn os_release(object: *mut c_void);
    pub fn os_log_type_enabled(log: os_log_t, level: os_log_type_t) -> bool;
}

/// Wrappers defined in wrapper.c because most of the os_log_* APIs are macros.
extern "C" {
    pub fn wrapped_get_default_log() -> os_log_t;
    pub fn wrapped_os_log_with_type(log: os_log_t, log_type: os_log_type_t, message: *const c_char);
    pub fn wrapped_os_log_debug(log: os_log_t, message: *const c_char);
    pub fn wrapped_os_log_info(log: os_log_t, message: *const c_char);
    pub fn wrapped_os_log_default(log: os_log_t, message: *const c_char);
    pub fn wrapped_os_log_error(log: os_log_t, message: *const c_char);
    pub fn wrapped_os_log_fault(log: os_log_t, message: *const c_char);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn test_create_and_release() {
        let subsystem = CString::new("com.example.test").unwrap();
        let category = CString::new("category").unwrap();
        let log = unsafe { os_log_create(subsystem.as_ptr(), category.as_ptr()) };
        assert!(!log.is_null());

        unsafe {
            os_release(log as *mut _);
        }
    }

    #[test]
    fn test_output_to_default_log() {
        let message = CString::new("Hello!").unwrap();

        unsafe {
            wrapped_os_log_debug(wrapped_get_default_log(), message.as_ptr());
            wrapped_os_log_info(wrapped_get_default_log(), message.as_ptr());
            wrapped_os_log_default(wrapped_get_default_log(), message.as_ptr());
            wrapped_os_log_error(wrapped_get_default_log(), message.as_ptr());
            wrapped_os_log_fault(wrapped_get_default_log(), message.as_ptr());

            wrapped_os_log_with_type(
                wrapped_get_default_log(),
                OS_LOG_TYPE_DEBUG,
                message.as_ptr(),
            );
            wrapped_os_log_with_type(
                wrapped_get_default_log(),
                OS_LOG_TYPE_INFO,
                message.as_ptr(),
            );
            wrapped_os_log_with_type(
                wrapped_get_default_log(),
                OS_LOG_TYPE_DEFAULT,
                message.as_ptr(),
            );
            wrapped_os_log_with_type(
                wrapped_get_default_log(),
                OS_LOG_TYPE_ERROR,
                message.as_ptr(),
            );
            wrapped_os_log_with_type(
                wrapped_get_default_log(),
                OS_LOG_TYPE_FAULT,
                message.as_ptr(),
            );
        }
    }

    #[test]
    fn test_output_to_custom_log() {
        let subsystem = CString::new("com.example.test").unwrap();
        let category = CString::new("category").unwrap();
        let log = unsafe { os_log_create(subsystem.as_ptr(), category.as_ptr()) };
        let message = CString::new("Hello!").unwrap();

        unsafe {
            wrapped_os_log_debug(log, message.as_ptr());
            wrapped_os_log_info(log, message.as_ptr());
            wrapped_os_log_default(log, message.as_ptr());
            wrapped_os_log_error(log, message.as_ptr());
            wrapped_os_log_fault(log, message.as_ptr());

            wrapped_os_log_with_type(log, OS_LOG_TYPE_DEBUG, message.as_ptr());
            wrapped_os_log_with_type(log, OS_LOG_TYPE_INFO, message.as_ptr());
            wrapped_os_log_with_type(log, OS_LOG_TYPE_DEFAULT, message.as_ptr());
            wrapped_os_log_with_type(log, OS_LOG_TYPE_ERROR, message.as_ptr());
            wrapped_os_log_with_type(log, OS_LOG_TYPE_FAULT, message.as_ptr());

            os_release(log as *mut _);
        }
    }
}
