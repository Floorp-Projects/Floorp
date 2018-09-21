// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::base::TCFType;
use core_foundation::string::CFString;
use core_foundation::url::CFURLRef;

pub fn copy_available_font_family_names() -> CFArray<CFString> {
    unsafe {
        TCFType::wrap_under_create_rule(CTFontManagerCopyAvailableFontFamilyNames())
    }
}

extern {
    /*
     * CTFontManager.h
     */

    // Incomplete function bindings are mostly related to CoreText font matching, which
    // we implement in a platform-independent manner using FontMatcher.

    //pub fn CTFontManagerCompareFontFamilyNames
    pub fn CTFontManagerCopyAvailableFontURLs() -> CFArrayRef;
    pub fn CTFontManagerCopyAvailableFontFamilyNames() -> CFArrayRef;
    pub fn CTFontManagerCopyAvailablePostScriptNames() -> CFArrayRef;
    pub fn CTFontManagerCreateFontDescriptorsFromURL(fileURL: CFURLRef) -> CFArrayRef;
    //pub fn CTFontManagerCreateFontRequestRunLoopSource
    //pub fn CTFontManagerEnableFontDescriptors
    //pub fn CTFontManagerGetAutoActivationSetting
    //pub fn CTFontManagerGetScopeForURL
    //pub fn CTFontManagerGetAutoActivationSetting
    //pub fn CTFontManagerGetScopeForURL
    pub fn CTFontManagerIsSupportedFont(fontURL: CFURLRef) -> bool;
    //pub fn CTFontManagerRegisterFontsForURL
    //pub fn CTFontManagerRegisterFontsForURLs
    //pub fn CTFontManagerRegisterGraphicsFont
    //pub fn CTFontManagerSetAutoActivationSetting
    //pub fn CTFontManagerUnregisterFontsForURL
    //pub fn CTFontManagerUnregisterFontsForURLs
    //pub fn CTFontManagerUnregisterGraphicsFont
}
