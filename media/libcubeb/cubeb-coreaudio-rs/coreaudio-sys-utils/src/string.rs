use core_foundation_sys::base::{
    kCFAllocatorDefault, kCFAllocatorNull, Boolean, CFIndex, CFRange, CFRelease,
};
use core_foundation_sys::string::{
    kCFStringEncodingUTF8, CFStringCreateWithBytes, CFStringCreateWithBytesNoCopy,
    CFStringGetBytes, CFStringGetLength, CFStringRef,
};
use std::ffi::CString;

pub fn cfstringref_from_static_string(string: &'static str) -> coreaudio_sys::CFStringRef {
    // Set deallocator to kCFAllocatorNull to prevent the the memory of the parameter `string`
    // from being released by CFRelease. We manage the string memory by ourselves.
    let cfstringref = unsafe {
        CFStringCreateWithBytesNoCopy(
            kCFAllocatorDefault,
            string.as_ptr(),
            string.len() as CFIndex,
            kCFStringEncodingUTF8,
            false as Boolean,
            kCFAllocatorNull,
        )
    };
    cfstringref as coreaudio_sys::CFStringRef
}

pub fn cfstringref_from_string(string: &str) -> coreaudio_sys::CFStringRef {
    let cfstringref = unsafe {
        CFStringCreateWithBytes(
            kCFAllocatorDefault,
            string.as_ptr(),
            string.len() as CFIndex,
            kCFStringEncodingUTF8,
            false as Boolean,
        )
    };
    cfstringref as coreaudio_sys::CFStringRef
}

#[derive(Debug)]
pub struct StringRef(CFStringRef);
impl StringRef {
    pub fn new(string_ref: CFStringRef) -> Self {
        assert!(!string_ref.is_null());
        Self(string_ref)
    }

    pub fn to_string(&self) -> String {
        String::from_utf8(utf8_from_cfstringref(self.0)).expect("convert bytes to a String")
    }

    pub fn into_string(self) -> String {
        self.to_string()
    }

    pub fn to_cstring(&self) -> CString {
        unsafe {
            // Assume that bytes doesn't contain `0` in the middle.
            CString::from_vec_unchecked(utf8_from_cfstringref(self.0))
        }
    }

    pub fn into_cstring(self) -> CString {
        self.to_cstring()
    }

    pub fn get_raw(&self) -> CFStringRef {
        self.0
    }
}

impl Drop for StringRef {
    fn drop(&mut self) {
        use std::os::raw::c_void;
        unsafe { CFRelease(self.0 as *mut c_void) };
    }
}

fn utf8_from_cfstringref(string_ref: CFStringRef) -> Vec<u8> {
    use std::ptr;

    assert!(!string_ref.is_null());

    let length: CFIndex = unsafe { CFStringGetLength(string_ref) };
    assert!(length > 0);

    // Get the buffer size of the string.
    let range: CFRange = CFRange {
        location: 0,
        length,
    };
    let mut size: CFIndex = 0;
    let mut converted_chars: CFIndex = unsafe {
        CFStringGetBytes(
            string_ref,
            range,
            kCFStringEncodingUTF8,
            0,
            false as Boolean,
            ptr::null_mut() as *mut u8,
            0,
            &mut size,
        )
    };
    assert!(converted_chars > 0 && size > 0);

    // Then, allocate the buffer with the required size and actually copy data into it.
    let mut buffer = vec![b'\x00'; size as usize];
    converted_chars = unsafe {
        CFStringGetBytes(
            string_ref,
            range,
            kCFStringEncodingUTF8,
            0,
            false as Boolean,
            buffer.as_mut_ptr(),
            size,
            ptr::null_mut() as *mut CFIndex,
        )
    };
    assert!(converted_chars > 0);

    buffer
}

#[cfg(test)]
mod test {
    use super::*;

    const STATIC_STRING: &str = "static string for testing";

    #[test]
    fn test_create_static_cfstring_ref() {
        let stringref =
            StringRef::new(cfstringref_from_static_string(STATIC_STRING) as CFStringRef);
        assert_eq!(STATIC_STRING, stringref.to_string());
        assert_eq!(
            CString::new(STATIC_STRING).unwrap(),
            stringref.into_cstring()
        );
        // TODO: Find a way to check the string's inner pointer is same.
    }

    #[test]
    fn test_create_cfstring_ref() {
        let expected = "Rustaceans 🦀";
        let stringref = StringRef::new(cfstringref_from_string(expected) as CFStringRef);
        assert_eq!(expected, stringref.to_string());
        assert_eq!(CString::new(expected).unwrap(), stringref.into_cstring());
        // TODO: Find a way to check the string's inner pointer is different.
    }
}
