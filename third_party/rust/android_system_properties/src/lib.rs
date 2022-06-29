//! A thin rust wrapper for Andoird system properties.
//!
//! This crate is similar to the `android-properties` crate with the exception that
//! the necessary Android libc symbols are loaded dynamically instead of linked
//! statically. In practice this means that the same binary will work with old and
//! new versions of Android, even though the API for reading system properties changed
//! around Android L.
use std::{
    ffi::{CStr, CString},
    os::raw::{c_char, c_int, c_void},
};

#[cfg(target_os = "android")]
use std::mem;

unsafe fn property_callback(payload: *mut String, _name: *const c_char, value: *const c_char, _serial: u32) {
    let cvalue = CStr::from_ptr(value);
    (*payload) = cvalue.to_str().unwrap().to_string();
}

type Callback = unsafe fn(*mut String, *const c_char, *const c_char, u32);

type SystemPropertyGetFn = unsafe extern "C" fn(*const c_char, *mut c_char) -> c_int;
type SystemPropertyFindFn = unsafe extern "C" fn(*const c_char) -> *const c_void;
type SystemPropertyReadCallbackFn = unsafe extern "C" fn(*const c_void, Callback, *mut String) -> *const c_void;

#[derive(Debug)]
/// An object that can retrieve android system properties.
///
/// ## Example
///
/// ```
/// use android_system_properties::AndroidSystemProperties;
///
/// let properties = AndroidSystemProperties::new();
///
/// if let Some(value) = properties.get("persist.sys.timezone") {
///    println!("{}", value);
/// }
/// ```
pub struct AndroidSystemProperties {
    libc_so: *mut c_void,
    get_fn: Option<SystemPropertyGetFn>,
    find_fn: Option<SystemPropertyFindFn>,
    read_callback_fn: Option<SystemPropertyReadCallbackFn>,
}

impl AndroidSystemProperties {
    #[cfg(not(target_os = "android"))]
    /// Create an entry point for accessing Android properties.
    pub fn new() -> Self {
        AndroidSystemProperties {
            libc_so: std::ptr::null_mut(),
            find_fn: None,
            read_callback_fn: None,
            get_fn: None,
        }
    }

    #[cfg(target_os = "android")]
    /// Create an entry point for accessing Android properties.
    pub fn new() -> Self {
        let libc_name = CString::new("libc.so").unwrap();
        let libc_so = unsafe { libc::dlopen(libc_name.as_ptr(), libc::RTLD_NOLOAD) };

        let mut properties = AndroidSystemProperties {
            libc_so,
            find_fn: None,
            read_callback_fn: None,
            get_fn: None,
        };

        if libc_so.is_null() {
            return properties;
        }


        unsafe fn load_fn(libc_so: *mut c_void, name: &str) -> Option<*const c_void> {
            let cname = CString::new(name).unwrap();
            let fn_ptr = libc::dlsym(libc_so, cname.as_ptr());

            if fn_ptr.is_null() {
                return None;
            }

            Some(fn_ptr)
        }

        unsafe {
            properties.read_callback_fn = load_fn(libc_so, "__system_property_read_callback")
                .map(|raw| mem::transmute::<*const c_void, SystemPropertyReadCallbackFn>(raw));

            properties.find_fn = load_fn(libc_so, "__system_property_find")
                .map(|raw| mem::transmute::<*const c_void, SystemPropertyFindFn>(raw));

            // Fallback for old versions of Android.
            if properties.read_callback_fn.is_none() || properties.find_fn.is_none() {
                properties.get_fn = load_fn(libc_so, "__system_property_get")
                    .map(|raw| mem::transmute::<*const c_void, SystemPropertyGetFn>(raw));
            }
        }

        properties
    }

    /// Retrieve a system property.
    ///
    /// Returns None if the operation fails.
    pub fn get(&self, name: &str) -> Option<String> {
        let cname = CString::new(name).unwrap();

        // If available, use the recommended approach to accessing properties (Android L and onward).
        if let (Some(find_fn), Some(read_callback_fn)) = (self.find_fn, self.read_callback_fn) {
            let info = unsafe { (find_fn)(cname.as_ptr()) };

            if info.is_null() {
                return None;
            }

            let mut result = String::new();

            unsafe {
                (read_callback_fn)(info, property_callback, &mut result);
            }

            return Some(result);
        }

        // Fall back to the older approach.
        if let Some(get_fn) = self.get_fn {
            // The constant is PROP_VALUE_MAX in Android's libc/include/sys/system_properties.h
            const PROPERTY_VALUE_MAX: usize = 92;
            let cvalue = CString::new(Vec::with_capacity(PROPERTY_VALUE_MAX)).unwrap();
            let raw = cvalue.into_raw();

            let len = unsafe { (get_fn)(cname.as_ptr(), raw) };

            let bytes = unsafe {
                let raw: *mut u8 = std::mem::transmute(raw); // Cast from *mut i8.
                Vec::from_raw_parts(raw, len as usize, PROPERTY_VALUE_MAX)
            };

            if len > 0 {
                String::from_utf8(bytes).ok()
            } else {
                None
            }
        } else {
            None
        }
    }
}

impl Drop for AndroidSystemProperties {
    fn drop(&mut self) {
        if !self.libc_so.is_null() {
            unsafe {
                libc::dlclose(self.libc_so);
            }    
        }
    }
}
