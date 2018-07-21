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
use font_manager::{CTFontManagerCopyAvailableFontFamilyNames, CTFontManagerCopyAvailablePostScriptNames};

use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::base::{CFTypeID, TCFType};
use core_foundation::dictionary::{CFDictionary, CFDictionaryRef};
use core_foundation::number::CFNumber;
use core_foundation::set::CFSet;
use core_foundation::string::{CFString, CFStringRef};

use std::os::raw::c_void;

#[repr(C)]
pub struct __CTFontCollection(c_void);

pub type CTFontCollectionRef = *const __CTFontCollection;

declare_TCFType! {
    CTFontCollection, CTFontCollectionRef
}
impl_TCFType!(CTFontCollection, CTFontCollectionRef, CTFontCollectionGetTypeID);
impl_CFTypeDescription!(CTFontCollection);


impl CTFontCollection {
    pub fn get_descriptors(&self) -> CFArray<CTFontDescriptor> {
        // surprise! this function follows the Get rule, despite being named *Create*.
        // So we have to addRef it to avoid CTFontCollection from double freeing it later.
        unsafe {
            CFArray::wrap_under_get_rule(CTFontCollectionCreateMatchingFontDescriptors(self.0))
        }
    }
}

pub fn new_from_descriptors(descs: &CFArray<CTFontDescriptor>) -> CTFontCollection {
    unsafe {
        let key = CFString::wrap_under_get_rule(kCTFontCollectionRemoveDuplicatesOption);
        let value = CFNumber::from(1i64);
        let options = CFDictionary::from_CFType_pairs(&[ (key.as_CFType(), value.as_CFType()) ]);
        let font_collection_ref =
            CTFontCollectionCreateWithFontDescriptors(descs.as_concrete_TypeRef(),
                                                      options.as_concrete_TypeRef());
        CTFontCollection::wrap_under_create_rule(font_collection_ref)
    }
}

pub fn create_for_all_families() -> CTFontCollection {
    unsafe {
        let key = CFString::wrap_under_get_rule(kCTFontCollectionRemoveDuplicatesOption);
        let value = CFNumber::from(1i64);
        let options = CFDictionary::from_CFType_pairs(&[ (key.as_CFType(), value.as_CFType()) ]);
        let font_collection_ref =
            CTFontCollectionCreateFromAvailableFonts(options.as_concrete_TypeRef());
        CTFontCollection::wrap_under_create_rule(font_collection_ref)
    }
}

pub fn create_for_family(family: &str) -> Option<CTFontCollection> {
    use font_descriptor::kCTFontFamilyNameAttribute;

    unsafe {
        let family_attr = CFString::wrap_under_get_rule(kCTFontFamilyNameAttribute);
        let family_name: CFString = family.parse().unwrap();
        let specified_attrs = CFDictionary::from_CFType_pairs(&[
            (family_attr.clone(), family_name.as_CFType())
        ]);

        let wildcard_desc: CTFontDescriptor =
            font_descriptor::new_from_attributes(&specified_attrs);
        let mandatory_attrs = CFSet::from_slice(&[ family_attr.as_CFType() ]);
        let matched_descs = CTFontDescriptorCreateMatchingFontDescriptors(
                wildcard_desc.as_concrete_TypeRef(),
                mandatory_attrs.as_concrete_TypeRef());
        if matched_descs.is_null() {
            return None;
        }
        let matched_descs = CFArray::wrap_under_create_rule(matched_descs);
        // I suppose one doesn't even need the CTFontCollection object at this point.
        // But we stick descriptors into and out of it just to provide a nice wrapper API.
        Some(new_from_descriptors(&matched_descs))
    }
}

pub fn get_family_names() -> CFArray<CFString> {
    unsafe {
        CFArray::wrap_under_create_rule(CTFontManagerCopyAvailableFontFamilyNames())
    }
}

pub fn get_postscript_names() -> CFArray<CFString> {
    unsafe {
        CFArray::wrap_under_create_rule(CTFontManagerCopyAvailablePostScriptNames())
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
