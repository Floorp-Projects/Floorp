use coreaudio_sys::*;
use std::os::raw::c_void;

pub struct CFMutableDictRef(CFMutableDictionaryRef);

impl CFMutableDictRef {
    pub fn add_value<K, V>(&self, key: *const K, value: *const V) {
        assert!(!self.0.is_null());
        unsafe {
            CFDictionaryAddValue(self.0, key as *const c_void, value as *const c_void);
        }
    }
}

impl Default for CFMutableDictRef {
    fn default() -> Self {
        let dict = unsafe {
            CFDictionaryCreateMutable(
                kCFAllocatorDefault,
                0,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks,
            )
        };
        assert!(!dict.is_null());
        Self(dict)
    }
}

impl Drop for CFMutableDictRef {
    fn drop(&mut self) {
        assert!(!self.0.is_null());
        unsafe {
            CFRelease(self.0 as *const c_void);
        }
    }
}
