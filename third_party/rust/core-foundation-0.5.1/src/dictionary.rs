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

use core_foundation_sys::base::{CFTypeRef, kCFAllocatorDefault};
use libc::c_void;
use std::mem;
use std::ptr;

use base::{CFType, CFIndexConvertible, TCFType, TCFTypeRef};


declare_TCFType!{
    /// An immutable dictionary of key-value pairs.
    CFDictionary, CFDictionaryRef
}
impl_TCFType!(CFDictionary, CFDictionaryRef, CFDictionaryGetTypeID);
impl_CFTypeDescription!(CFDictionary);

impl CFDictionary {
    pub fn from_CFType_pairs<K: TCFType, V: TCFType>(pairs: &[(K, V)]) -> CFDictionary {
        let (keys, values): (Vec<CFTypeRef>, Vec<CFTypeRef>) = pairs
            .iter()
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
        unsafe { CFDictionaryContainsKey(self.0, key) != 0 }
    }

    /// Similar to `contains_key` but acts on a higher level, automatically converting from any
    /// `TCFType` to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn contains_key2<K: TCFType>(&self, key: &K) -> bool {
        self.contains_key(key.as_concrete_TypeRef().as_void_ptr())
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
    pub fn find2<K: TCFType>(&self, key: &K) -> Option<*const c_void> {
        self.find(key.as_concrete_TypeRef().as_void_ptr())
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

declare_TCFType!{
    /// An mutable dictionary of key-value pairs.
    CFMutableDictionary, CFMutableDictionaryRef
}

impl_TCFType!(CFMutableDictionary, CFMutableDictionaryRef, CFDictionaryGetTypeID);
impl_CFTypeDescription!(CFMutableDictionary);

impl CFMutableDictionary {
    pub fn new() -> Self {
        Self::with_capacity(0)
    }

    pub fn with_capacity(capacity: isize) -> Self {
        unsafe {
            let dictionary_ref = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                           capacity as _,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
            TCFType::wrap_under_create_rule(dictionary_ref)
        }
    }

    pub fn copy_with_capacity(&self, capacity: isize) -> Self {
        unsafe {
            let dictionary_ref = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, capacity as _, self.0);
            TCFType::wrap_under_get_rule(dictionary_ref)
        }
    }

    pub fn from_CFType_pairs<K: TCFType, V: TCFType>(pairs: &[(K, V)]) -> CFMutableDictionary {
        let result = Self::with_capacity(pairs.len() as _);
        unsafe {
            for &(ref key, ref value) in pairs {
                result.add(key.as_CFTypeRef(), value.as_CFTypeRef());
            }
        }
        result
    }

    // Immutable interface

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
    pub fn contains_key2<K: TCFType>(&self, key: &K) -> bool {
        self.contains_key(key.as_concrete_TypeRef().as_void_ptr())
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
    pub fn find2<K: TCFType>(&self, key: &K) -> Option<*const c_void> {
        self.find(key.as_concrete_TypeRef().as_void_ptr())
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

    // Mutable interface

    /// Adds the key-value pair to the dictionary if no such key already exist.
    #[inline]
    pub unsafe fn add(&self, key: *const c_void, value: *const c_void) {
        CFDictionaryAddValue(self.0, key, value)
    }

    /// Similar to `add` but acts on a higher level, automatically converting from any `TCFType`
    /// to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn add2<K: TCFType, V: TCFType>(&self, key: &K, value: &V) {
        unsafe {
            self.add(
                key.as_concrete_TypeRef().as_void_ptr(),
                value.as_concrete_TypeRef().as_void_ptr(),
            )
        }
    }

    /// Sets the value of the key in the dictionary.
    #[inline]
    pub unsafe fn set(&self, key: *const c_void, value: *const c_void) {
        CFDictionarySetValue(self.0, key, value)
    }

    /// Similar to `set` but acts on a higher level, automatically converting from any `TCFType`
    /// to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn set2<K: TCFType, V: TCFType>(&self, key: &K, value: &V) {
        unsafe {
            self.set(
                key.as_concrete_TypeRef().as_void_ptr(),
                value.as_concrete_TypeRef().as_void_ptr(),
            )
        }
    }

    /// Replaces the value of the key in the dictionary.
    #[inline]
    pub unsafe fn replace(&self, key: *const c_void, value: *const c_void) {
        CFDictionaryReplaceValue(self.0, key, value)
    }

    /// Similar to `replace` but acts on a higher level, automatically converting from any `TCFType`
    /// to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn replace2<K: TCFType, V: TCFType>(&self, key: &K, value: &V) {
        unsafe {
            self.replace(
                key.as_concrete_TypeRef().as_void_ptr(),
                value.as_concrete_TypeRef().as_void_ptr(),
            )
        }
    }

    /// Removes the value of the key from the dictionary.
    #[inline]
    pub unsafe fn remove(&self, key: *const c_void) {
        CFDictionaryRemoveValue(self.0, key);
    }

    /// Similar to `remove` but acts on a higher level, automatically converting from any `TCFType`
    /// to the raw pointer of its concrete TypeRef.
    #[inline]
    pub fn remove2<K: TCFType>(&self, key: &K) {
        unsafe { self.remove(key.as_concrete_TypeRef().as_void_ptr()) }
    }

    #[inline]
    pub fn remove_all(&self) {
        unsafe { CFDictionaryRemoveAllValues(self.0) }
    }
}


#[cfg(test)]
pub mod test {
    use super::*;
    use base::TCFType;
    use boolean::{CFBoolean, CFBooleanRef};
    use number::CFNumber;
    use string::CFString;


    #[test]
    fn dictionary() {
        let bar = CFString::from_static_string("Bar");
        let baz = CFString::from_static_string("Baz");
        let boo = CFString::from_static_string("Boo");
        let foo = CFString::from_static_string("Foo");
        let tru = CFBoolean::true_value();
        let n42 = CFNumber::from(42);

        let d = CFDictionary::from_CFType_pairs(&[
            (bar.as_CFType(), boo.as_CFType()),
            (baz.as_CFType(), tru.as_CFType()),
            (foo.as_CFType(), n42.as_CFType()),
        ]);

        let (v1, v2) = d.get_keys_and_values();
        assert!(v1 == &[bar.as_CFTypeRef(), baz.as_CFTypeRef(), foo.as_CFTypeRef()]);
        assert!(v2 == &[boo.as_CFTypeRef(), tru.as_CFTypeRef(), n42.as_CFTypeRef()]);
    }

    #[test]
    fn mutable_dictionary() {
        let bar = CFString::from_static_string("Bar");
        let baz = CFString::from_static_string("Baz");
        let boo = CFString::from_static_string("Boo");
        let foo = CFString::from_static_string("Foo");
        let tru = CFBoolean::true_value();
        let n42 = CFNumber::from(42);

        let d = CFMutableDictionary::new();
        d.add2(&bar, &boo);
        d.add2(&baz, &tru);
        d.add2(&foo, &n42);
        assert_eq!(d.len(), 3);

        let (v1, v2) = d.get_keys_and_values();
        assert!(v1 == &[bar.as_CFTypeRef(), baz.as_CFTypeRef(), foo.as_CFTypeRef()]);
        assert!(v2 == &[boo.as_CFTypeRef(), tru.as_CFTypeRef(), n42.as_CFTypeRef()]);

        d.remove2(&baz);
        assert_eq!(d.len(), 2);

        let (v1, v2) = d.get_keys_and_values();
        assert!(v1 == &[bar.as_CFTypeRef(), foo.as_CFTypeRef()]);
        assert!(v2 == &[boo.as_CFTypeRef(), n42.as_CFTypeRef()]);

        d.remove_all();
        assert_eq!(d.len(), 0)
    }

    #[test]
    fn dict_find2_and_contains_key2() {
        let dict = CFDictionary::from_CFType_pairs(&[
            (
                CFString::from_static_string("hello"),
                CFBoolean::true_value(),
            ),
        ]);
        let key = CFString::from_static_string("hello");
        let invalid_key = CFString::from_static_string("foobar");

        assert!(dict.contains_key2(&key));
        assert!(!dict.contains_key2(&invalid_key));

        let value = unsafe { CFBoolean::wrap_under_get_rule(dict.find2(&key).unwrap() as CFBooleanRef) };
        assert_eq!(value, CFBoolean::true_value());
        assert_eq!(dict.find2(&invalid_key), None);
    }
}
