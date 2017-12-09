// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Dictionaries of key-value pairs.

pub use core_foundation_sys::dictionary::*;
use core_foundation_sys::base::CFRelease;
use core_foundation_sys::base::{CFTypeRef, kCFAllocatorDefault};
use libc::c_void;
use std::mem;
use std::ptr;

use base::{CFType, CFIndexConvertible, TCFType};

/// An immutable dictionary of key-value pairs.
pub struct CFDictionary(CFDictionaryRef);

impl Drop for CFDictionary {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl_TCFType!(CFDictionary, CFDictionaryRef, CFDictionaryGetTypeID);
impl_CFTypeDescription!(CFDictionary);

impl CFDictionary {
    pub fn from_CFType_pairs<R1, R2, K, V>(pairs: &[(K, V)]) -> CFDictionary
            where K: TCFType<R1>, V: TCFType<R2> {
        let (keys, values): (Vec<CFTypeRef>,Vec<CFTypeRef>) =
            pairs.iter()
            .map(|&(ref key, ref value)| (key.as_CFTypeRef(), value.as_CFTypeRef()))
            .unzip();

        unsafe {
            let dictionary_ref = CFDictionaryCreate(kCFAllocatorDefault,
                                                    mem::transmute(keys.as_ptr()),
                                                    mem::transmute(values.as_ptr()),
                                                    keys.len().to_CFIndex(),
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks);
            TCFType::wrap_under_create_rule(dictionary_ref)
        }
    }

    #[inline]
    pub fn len(&self) -> usize {
        unsafe {
            CFDictionaryGetCount(self.0) as usize
        }
    }

    #[inline]
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    #[inline]
    pub fn contains_key(&self, key: *const c_void) -> bool {
        unsafe {
            CFDictionaryContainsKey(self.0, key) != 0
        }
    }

    /// Similar to `contains_key` but acts on a higher level, automatically converting from any
    /// `TCFType` to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn contains_key2<X, K: TCFType<*const X>>(&self, key: &K) -> bool {
        self.contains_key(key.as_concrete_TypeRef() as *const c_void)
    }

    #[inline]
    pub fn find(&self, key: *const c_void) -> Option<*const c_void> {
        unsafe {
            let mut value: *const c_void = ptr::null();
            if CFDictionaryGetValueIfPresent(self.0, key, &mut value) != 0 {
                Some(value)
            } else {
                None
            }
        }
    }

    /// Similar to `find` but acts on a higher level, automatically converting from any `TCFType`
    /// to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn find2<X, K: TCFType<*const X>>(&self, key: &K) -> Option<*const c_void> {
        self.find(key.as_concrete_TypeRef() as *const c_void)
    }

    /// # Panics
    ///
    /// Panics if the key is not present in the dictionary. Use `find` to get an `Option` instead
    /// of panicking.
    #[inline]
    pub fn get(&self, key: *const c_void) -> *const c_void {
        self.find(key).expect(&format!("No entry found for key {:p}", key))
    }

    /// A convenience function to retrieve `CFType` instances.
    #[inline]
    pub unsafe fn get_CFType(&self, key: *const c_void) -> CFType {
        let value: CFTypeRef = mem::transmute(self.get(key));
        TCFType::wrap_under_get_rule(value)
    }

    #[inline]
    pub unsafe fn set_value(&self, key: *const c_void, value: *const c_void) {
        CFDictionarySetValue(self.0, key, value)
    }

    pub fn get_keys_and_values(&self) -> (Vec<*const c_void>, Vec<*const c_void>) {
        let length = self.len();
        let mut keys = Vec::with_capacity(length);
        let mut values = Vec::with_capacity(length);

        unsafe {
            CFDictionaryGetKeysAndValues(self.0, keys.as_mut_ptr(), values.as_mut_ptr());
            keys.set_len(length);
            values.set_len(length);
        }

        (keys, values)
    }
}
