// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use font_descriptor;
use font_descriptor::{CTFontDescriptor, CTFontDescriptorCreateMatchingFontDescriptors};
use font_manager::CTFontManagerCopyAvailableFontFamilyNames;

use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::base::{CFRelease, CFRetain, CFTypeID, CFTypeRef, TCFType};
use core_foundation::dictionary::{CFDictionary, CFDictionaryRef};
use core_foundation::number::CFNumber;
use core_foundation::set::CFSet;
use core_foundation::string::{CFString, CFStringRef};

use libc::c_void;
use std::mem;
use std::ptr;

#[repr(C)]
pub struct __CTFontCollection(c_void);

pub type CTFontCollectionRef = *const __CTFontCollection;

#[derive(Debug)]
pub struct CTFontCollection {
    obj: CTFontCollectionRef,
}

impl Drop for CTFontCollection {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.as_CFTypeRef())
        }
    }
}

impl TCFType<CTFontCollectionRef> for CTFontCollection {
    #[inline]
    fn as_concrete_TypeRef(&self) -> CTFontCollectionRef {
        self.obj
    }

    #[inline]
    unsafe fn wrap_under_get_rule(reference: CTFontCollectionRef) -> CTFontCollection {
        let reference: CTFontCollectionRef = mem::transmute(CFRetain(mem::transmute(reference)));
        TCFType::wrap_under_create_rule(reference)
    }

    #[inline]
    unsafe fn wrap_under_create_rule(obj: CTFontCollectionRef) -> CTFontCollection {
        CTFontCollection {
            obj: obj,
        }
    }

    #[inline]
    fn as_CFTypeRef(&self) -> CFTypeRef {
        unsafe {
            mem::transmute(self.as_concrete_TypeRef())
        }
    }

    #[inline]
    fn type_id() -> CFTypeID {
        unsafe {
            CTFontCollectionGetTypeID()
        }
    }
}

impl CTFontCollection {
    pub fn get_descriptors(&self) -> CFArray {
        // surprise! this function follows the Get rule, despite being named *Create*.
        // So we have to addRef it to avoid CTFontCollection from double freeing it later.
        unsafe {
            TCFType::wrap_under_get_rule(CTFontCollectionCreateMatchingFontDescriptors(self.obj))
        }
    }
}

pub fn new_from_descriptors(descs: &CFArray) -> CTFontCollection {
    unsafe {
        let key: CFString = TCFType::wrap_under_get_rule(kCTFontCollectionRemoveDuplicatesOption);
        let value = CFNumber::from_i64(1);
        let options = CFDictionary::from_CFType_pairs(&[ (key.as_CFType(), value.as_CFType()) ]);
        let font_collection_ref =
            CTFontCollectionCreateWithFontDescriptors(descs.as_concrete_TypeRef(),
                                                      options.as_concrete_TypeRef());
        TCFType::wrap_under_create_rule(font_collection_ref)
    }
}

pub fn create_for_all_families() -> CTFontCollection {
    unsafe {
        let key: CFString = TCFType::wrap_under_get_rule(kCTFontCollectionRemoveDuplicatesOption);
        let value = CFNumber::from_i64(1);
        let options = CFDictionary::from_CFType_pairs(&[ (key.as_CFType(), value.as_CFType()) ]);
        let font_collection_ref =
            CTFontCollectionCreateFromAvailableFonts(options.as_concrete_TypeRef());
        TCFType::wrap_under_create_rule(font_collection_ref)
    }
}

pub fn create_for_family(family: &str) -> Option<CTFontCollection> {
    use font_descriptor::kCTFontFamilyNameAttribute;

    unsafe {
        let family_attr: CFString = TCFType::wrap_under_get_rule(kCTFontFamilyNameAttribute);
        let family_name: CFString = family.parse().unwrap();
        let specified_attrs = CFDictionary::from_CFType_pairs(&[
            (family_attr.as_CFType(), family_name.as_CFType())
        ]);

        let wildcard_desc: CTFontDescriptor =
            font_descriptor::new_from_attributes(&specified_attrs);
        let mandatory_attrs = CFSet::from_slice(&[ family_attr.as_CFType() ]);
        let matched_descs = CTFontDescriptorCreateMatchingFontDescriptors(
                wildcard_desc.as_concrete_TypeRef(),
                mandatory_attrs.as_concrete_TypeRef());
        if matched_descs == ptr::null() {
            return None;
        }
        let matched_descs: CFArray = TCFType::wrap_under_create_rule(matched_descs);
        // I suppose one doesn't even need the CTFontCollection object at this point.
        // But we stick descriptors into and out of it just to provide a nice wrapper API.
        Some(new_from_descriptors(&matched_descs))
    }
}

pub fn get_family_names() -> CFArray {
    unsafe {
        TCFType::wrap_under_create_rule(CTFontManagerCopyAvailableFontFamilyNames())
    }
}

extern {
    /*
     * CTFontCollection.h
     */

    static kCTFontCollectionRemoveDuplicatesOption: CFStringRef;

    //fn CTFontCollectionCreateCopyWithFontDescriptors(original: CTFontCollectionRef,
    //                                                 descriptors: CFArrayRef,
    //                                                 options: CFDictionaryRef) -> CTFontCollectionRef;
    fn CTFontCollectionCreateFromAvailableFonts(options: CFDictionaryRef) -> CTFontCollectionRef;
    // this stupid function doesn't actually do any wildcard expansion; 
    // it just chooses the best match. Use
    // CTFontDescriptorCreateMatchingDescriptors instead.
    fn CTFontCollectionCreateMatchingFontDescriptors(collection: CTFontCollectionRef) -> CFArrayRef;
    fn CTFontCollectionCreateWithFontDescriptors(descriptors: CFArrayRef,
                                                 options: CFDictionaryRef) -> CTFontCollectionRef;
    //fn CTFontCollectionCreateMatchingFontDescriptorsSortedWithCallback;
    fn CTFontCollectionGetTypeID() -> CFTypeID;
}
