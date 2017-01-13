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

use libc::c_void;

use error::CFError;
use data::CFData;
use base::{TCFType};

pub use core_foundation_sys::propertylist::*;
use core_foundation_sys::error::CFErrorRef;
use core_foundation_sys::base::{kCFAllocatorDefault};

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

#[cfg(test)]
pub mod test {
    #[test]
    fn test_property_list_serialization() {
        use base::{TCFType, CFEqual};
        use boolean::CFBoolean;
        use number::number;
        use dictionary::CFDictionary;
        use string::CFString;
        use super::*;

        let bar = CFString::from_static_string("Bar");
        let baz = CFString::from_static_string("Baz");
        let boo = CFString::from_static_string("Boo");
        let foo = CFString::from_static_string("Foo");
        let tru = CFBoolean::true_value();
        let n42 = number(42);

        let dict1 = CFDictionary::from_CFType_pairs(&[(bar.as_CFType(), boo.as_CFType()),
                                                      (baz.as_CFType(), tru.as_CFType()),
                                                      (foo.as_CFType(), n42.as_CFType())]);

        let data = create_data(dict1.as_CFTypeRef(), kCFPropertyListXMLFormat_v1_0).unwrap();
        let (dict2, _) = create_with_data(data, kCFPropertyListImmutable).unwrap();
        unsafe {
            assert!(CFEqual(dict1.as_CFTypeRef(), dict2) == 1);
        }
    }
}
