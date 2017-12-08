// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Core Foundation property lists

use std::ptr;
use std::mem;

use libc::c_void;

use error::CFError;
use data::CFData;
use base::{CFType, TCFType};

pub use core_foundation_sys::propertylist::*;
use core_foundation_sys::error::CFErrorRef;
use core_foundation_sys::base::{CFGetRetainCount, CFGetTypeID, CFIndex, CFRelease, CFRetain,
                                CFShow, CFTypeID, kCFAllocatorDefault};

pub fn create_with_data(data: CFData,
                        options: CFPropertyListMutabilityOptions)
                        -> Result<(*const c_void, CFPropertyListFormat), CFError> {
    unsafe {
        let mut error: CFErrorRef = ptr::null_mut();
        let mut format: CFPropertyListFormat = 0;
        let property_list = CFPropertyListCreateWithData(kCFAllocatorDefault,
                                                         data.as_concrete_TypeRef(),
                                                         options,
                                                         &mut format,
                                                         &mut error);
        if property_list.is_null() {
            Err(TCFType::wrap_under_create_rule(error))
        } else {
            Ok((property_list, format))
        }
    }
}

pub fn create_data(property_list: *const c_void, format: CFPropertyListFormat) -> Result<CFData, CFError> {
    unsafe {
        let mut error: CFErrorRef = ptr::null_mut();
        let data_ref = CFPropertyListCreateData(kCFAllocatorDefault,
                                                property_list,
                                                format,
                                                0,
                                                &mut error);
        if data_ref.is_null() {
            Err(TCFType::wrap_under_create_rule(error))
        } else {
            Ok(TCFType::wrap_under_create_rule(data_ref))
        }
    }
}


/// Trait for all subclasses of [`CFPropertyList`].
///
/// [`CFPropertyList`]: struct.CFPropertyList.html
pub trait CFPropertyListSubClass<Raw>: TCFType<*const Raw> {
    /// Create an instance of the superclass type [`CFPropertyList`] for this instance.
    ///
    /// [`CFPropertyList`]: struct.CFPropertyList.html
    fn to_CFPropertyList(&self) -> CFPropertyList {
        unsafe { CFPropertyList::wrap_under_get_rule(self.as_concrete_TypeRef() as *const c_void) }
    }
}

impl CFPropertyListSubClass<::data::__CFData> for ::data::CFData {}
impl CFPropertyListSubClass<::string::__CFString> for ::string::CFString {}
impl CFPropertyListSubClass<::array::__CFArray> for ::array::CFArray {}
impl CFPropertyListSubClass<::dictionary::__CFDictionary> for ::dictionary::CFDictionary {}
impl CFPropertyListSubClass<::date::__CFDate> for ::date::CFDate {}
impl CFPropertyListSubClass<::number::__CFBoolean> for ::boolean::CFBoolean {}
impl CFPropertyListSubClass<::number::__CFNumber> for ::number::CFNumber {}

/// A CFPropertyList struct. This is superclass to [`CFData`], [`CFString`], [`CFArray`],
/// [`CFDictionary`], [`CFDate`], [`CFBoolean`], and [`CFNumber`].
///
/// This superclass type does not have its own `CFTypeID`, instead each instance has the `CFTypeID`
/// of the subclass it is an instance of. Thus, this type cannot implement the [`TCFType`] trait,
/// since it cannot implement the static [`TCFType::type_id()`] method.
///
/// [`CFData`]: ../data/struct.CFData.html
/// [`CFString`]: ../string/struct.CFString.html
/// [`CFArray`]: ../array/struct.CFArray.html
/// [`CFDictionary`]: ../dictionary/struct.CFDictionary.html
/// [`CFDate`]: ../date/struct.CFDate.html
/// [`CFBoolean`]: ../boolean/struct.CFBoolean.html
/// [`CFNumber`]: ../number/struct.CFNumber.html
/// [`TCFType`]: ../base/trait.TCFType.html
/// [`TCFType::type_id()`]: ../base/trait.TCFType.html#method.type_of
pub struct CFPropertyList(CFPropertyListRef);

impl Drop for CFPropertyList {
    fn drop(&mut self) {
        unsafe { CFRelease(self.as_CFTypeRef()) }
    }
}

impl CFPropertyList {
    #[inline]
    pub fn as_concrete_TypeRef(&self) -> CFPropertyListRef {
        self.0
    }

    #[inline]
    pub unsafe fn wrap_under_get_rule(reference: CFPropertyListRef) -> CFPropertyList {
        let reference = mem::transmute(CFRetain(mem::transmute(reference)));
        CFPropertyList(reference)
    }

    #[inline]
    pub fn as_CFType(&self) -> CFType {
        unsafe { CFType::wrap_under_get_rule(self.as_CFTypeRef()) }
    }

    #[inline]
    pub fn as_CFTypeRef(&self) -> ::core_foundation_sys::base::CFTypeRef {
        unsafe { mem::transmute(self.as_concrete_TypeRef()) }
    }

    #[inline]
    pub unsafe fn wrap_under_create_rule(obj: CFPropertyListRef) -> CFPropertyList {
        CFPropertyList(obj)
    }

    /// Returns the reference count of the object. It is unwise to do anything other than test
    /// whether the return value of this method is greater than zero.
    #[inline]
    pub fn retain_count(&self) -> CFIndex {
        unsafe { CFGetRetainCount(self.as_CFTypeRef()) }
    }

    /// Returns the type ID of this object. Will be one of CFData, CFString, CFArray, CFDictionary,
    /// CFDate, CFBoolean, or CFNumber.
    #[inline]
    pub fn type_of(&self) -> CFTypeID {
        unsafe { CFGetTypeID(self.as_CFTypeRef()) }
    }

    /// Writes a debugging version of this object on standard error.
    pub fn show(&self) {
        unsafe { CFShow(self.as_CFTypeRef()) }
    }

    /// Returns true if this value is an instance of another type.
    #[inline]
    pub fn instance_of<OtherConcreteTypeRef, OtherCFType: TCFType<OtherConcreteTypeRef>>(
        &self,
    ) -> bool {
        self.type_of() == <OtherCFType as TCFType<_>>::type_id()
    }
}

impl Clone for CFPropertyList {
    #[inline]
    fn clone(&self) -> CFPropertyList {
        unsafe { CFPropertyList::wrap_under_get_rule(self.0) }
    }
}

impl PartialEq for CFPropertyList {
    #[inline]
    fn eq(&self, other: &CFPropertyList) -> bool {
        self.as_CFType().eq(&other.as_CFType())
    }
}

impl Eq for CFPropertyList {}

impl CFPropertyList {
    /// Try to downcast the [`CFPropertyList`] to a subclass. Checking if the instance is the correct
    /// subclass happens at runtime and an error is returned if it is not the correct type.
    /// Works similar to [`Box::downcast`].
    ///
    /// # Examples
    ///
    /// ```
    /// # use core_foundation::string::CFString;
    /// # use core_foundation::propertylist::{CFPropertyList, CFPropertyListSubClass};
    /// #
    /// // Create a string.
    /// let string: CFString = CFString::from_static_string("FooBar");
    /// // Cast it up to a property list.
    /// let propertylist: CFPropertyList = string.to_CFPropertyList();
    /// // Cast it down again.
    /// assert!(propertylist.downcast::<_, CFString>().unwrap().to_string() == "FooBar");
    /// ```
    ///
    /// [`CFPropertyList`]: struct.CFPropertyList.html
    /// [`Box::downcast`]: https://doc.rust-lang.org/std/boxed/struct.Box.html#method.downcast
    pub fn downcast<Raw, T: CFPropertyListSubClass<Raw>>(&self) -> Option<T> {
        if self.instance_of::<_, T>() {
            Some(unsafe { T::wrap_under_get_rule(self.0 as *const Raw) })
        } else {
            None
        }
    }
}



#[cfg(test)]
pub mod test {
    use super::*;
    use string::CFString;
    use boolean::CFBoolean;

    #[test]
    fn test_property_list_serialization() {
        use base::{TCFType, CFEqual};
        use boolean::CFBoolean;
        use number::CFNumber;
        use dictionary::CFDictionary;
        use string::CFString;
        use super::*;

        let bar = CFString::from_static_string("Bar");
        let baz = CFString::from_static_string("Baz");
        let boo = CFString::from_static_string("Boo");
        let foo = CFString::from_static_string("Foo");
        let tru = CFBoolean::true_value();
        let n42 = CFNumber::from(42);

        let dict1 = CFDictionary::from_CFType_pairs(&[(bar.as_CFType(), boo.as_CFType()),
                                                      (baz.as_CFType(), tru.as_CFType()),
                                                      (foo.as_CFType(), n42.as_CFType())]);

        let data = create_data(dict1.as_CFTypeRef(), kCFPropertyListXMLFormat_v1_0).unwrap();
        let (dict2, _) = create_with_data(data, kCFPropertyListImmutable).unwrap();
        unsafe {
            assert!(CFEqual(dict1.as_CFTypeRef(), dict2) == 1);
        }
    }

    #[test]
    fn downcast_string() {
        let propertylist = CFString::from_static_string("Bar").to_CFPropertyList();
        assert!(propertylist.downcast::<_, CFString>().unwrap().to_string() == "Bar");
        assert!(propertylist.downcast::<_, CFBoolean>().is_none());
    }

    #[test]
    fn downcast_boolean() {
        let propertylist = CFBoolean::true_value().to_CFPropertyList();
        assert!(propertylist.downcast::<_, CFBoolean>().is_some());
        assert!(propertylist.downcast::<_, CFString>().is_none());
    }
}
