// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_upper_case_globals)]

use font_descriptor;
use font_descriptor::{CTFontDescriptor, CTFontDescriptorRef, CTFontOrientation};
use font_descriptor::{CTFontSymbolicTraits, CTFontTraits, SymbolicTraitAccessors, TraitAccessors};

use core_foundation::array::{CFArray, CFArrayRef};
use core_foundation::base::{CFIndex, CFOptionFlags, CFTypeID, CFTypeRef, TCFType};
use core_foundation::data::{CFData, CFDataRef};
use core_foundation::dictionary::{CFDictionary, CFDictionaryRef};
use core_foundation::number::CFNumber;
use core_foundation::string::{CFString, CFStringRef, UniChar};
use core_foundation::url::{CFURL, CFURLRef};
use core_graphics::base::CGFloat;
use core_graphics::context::CGContext;
use core_graphics::font::{CGGlyph, CGFont};
use core_graphics::geometry::{CGAffineTransform, CGPoint, CGRect, CGSize};
use core_graphics::path::CGPath;

use foreign_types::ForeignType;
use libc::{self, size_t, c_void};
use std::ptr;

type CGContextRef = *mut <CGContext as ForeignType>::CType;
type CGFontRef = *mut <CGFont as ForeignType>::CType;
type CGPathRef = *mut <CGPath as ForeignType>::CType;

pub type CTFontUIFontType = u32;
// kCTFontNoFontType: CTFontUIFontType = -1;
pub const kCTFontUserFontType: CTFontUIFontType = 0;
pub const kCTFontUserFixedPitchFontType: CTFontUIFontType = 1;
pub const kCTFontSystemFontType: CTFontUIFontType = 2;
pub const kCTFontEmphasizedSystemFontType: CTFontUIFontType = 3;
pub const kCTFontSmallSystemFontType: CTFontUIFontType = 4;
pub const kCTFontSmallEmphasizedSystemFontType: CTFontUIFontType = 5;
pub const kCTFontMiniSystemFontType: CTFontUIFontType = 6;
pub const kCTFontMiniEmphasizedSystemFontType: CTFontUIFontType = 7;
pub const kCTFontViewsFontType: CTFontUIFontType = 8;
pub const kCTFontApplicationFontType: CTFontUIFontType = 9;
pub const kCTFontLabelFontType: CTFontUIFontType = 10;
pub const kCTFontMenuTitleFontType: CTFontUIFontType = 11;
pub const kCTFontMenuItemFontType: CTFontUIFontType = 12;
pub const kCTFontMenuItemMarkFontType: CTFontUIFontType = 13;
pub const kCTFontMenuItemCmdKeyFontType: CTFontUIFontType = 14;
pub const kCTFontWindowTitleFontType: CTFontUIFontType = 15;
pub const kCTFontPushButtonFontType: CTFontUIFontType = 16;
pub const kCTFontUtilityWindowTitleFontType: CTFontUIFontType = 17;
pub const kCTFontAlertHeaderFontType: CTFontUIFontType = 18;
pub const kCTFontSystemDetailFontType: CTFontUIFontType = 19;
pub const kCTFontEmphasizedSystemDetailFontType: CTFontUIFontType = 20;
pub const kCTFontToolbarFontType: CTFontUIFontType = 21;
pub const kCTFontSmallToolbarFontType: CTFontUIFontType = 22;
pub const kCTFontMessageFontType: CTFontUIFontType = 23;
pub const kCTFontPaletteFontType: CTFontUIFontType = 24;
pub const kCTFontToolTipFontType: CTFontUIFontType = 25;
pub const kCTFontControlContentFontType: CTFontUIFontType = 26;

pub type CTFontTableTag = u32;
// TODO: create bindings for enum with 'chars' values

pub type CTFontTableOptions = u32;
pub const kCTFontTableOptionsNoOptions: CTFontTableOptions = 0;
pub const kCTFontTableOptionsExcludeSynthetic: CTFontTableOptions = (1 << 0);

pub type CTFontOptions = CFOptionFlags;
pub const kCTFontOptionsDefault: CTFontOptions = 0;
pub const kCTFontOptionsPreventAutoActivation: CTFontOptions = (1 << 0);
pub const kCTFontOptionsPreferSystemFont: CTFontOptions = (1 << 2);

#[repr(C)]
pub struct __CTFont(c_void);

pub type CTFontRef = *const __CTFont;

declare_TCFType! {
    CTFont, CTFontRef
}
impl_TCFType!(CTFont, CTFontRef, CTFontGetTypeID);
impl_CFTypeDescription!(CTFont);

pub fn new_from_CGFont(cgfont: &CGFont, pt_size: f64) -> CTFont {
    unsafe {
        let font_ref = CTFontCreateWithGraphicsFont(cgfont.as_ptr() as *mut _,
                                                    pt_size as CGFloat,
                                                    ptr::null(),
                                                    ptr::null());
        CTFont::wrap_under_create_rule(font_ref)
    }
}

pub fn new_from_CGFont_with_variations(cgfont: &CGFont,
                                       pt_size: f64,
                                       variations: &CFDictionary<CFString, CFNumber>)
                                       -> CTFont {
    unsafe {
        let font_desc = font_descriptor::new_from_variations(variations);
        let font_ref = CTFontCreateWithGraphicsFont(cgfont.as_ptr() as *mut _,
                                                    pt_size as CGFloat,
                                                    ptr::null(),
                                                    font_desc.as_concrete_TypeRef());
        CTFont::wrap_under_create_rule(font_ref)
    }
}

pub fn new_from_descriptor(desc: &CTFontDescriptor, pt_size: f64) -> CTFont {
    unsafe {
        let font_ref = CTFontCreateWithFontDescriptor(desc.as_concrete_TypeRef(),
                                                      pt_size as CGFloat,
                                                      ptr::null());
        CTFont::wrap_under_create_rule(font_ref)
    }
}

pub fn new_from_name(name: &str, pt_size: f64) -> Result<CTFont, ()> {
    unsafe {
        let name: CFString = name.parse().unwrap();
        let font_ref = CTFontCreateWithName(name.as_concrete_TypeRef(),
                                            pt_size as CGFloat,
                                            ptr::null());
        if font_ref.is_null() {
            Err(())
        } else {
            Ok(CTFont::wrap_under_create_rule(font_ref))
        }
    }
}

impl CTFont {
    // Properties
    pub fn symbolic_traits(&self) -> CTFontSymbolicTraits {
        unsafe {
            CTFontGetSymbolicTraits(self.0)
        }
    }
}

impl CTFont {
    // Creation methods
    pub fn copy_to_CGFont(&self) -> CGFont {
        unsafe {
            let cgfont_ref = CTFontCopyGraphicsFont(self.0, ptr::null_mut());
            CGFont::from_ptr(cgfont_ref as *mut _)
        }
    }

    pub fn clone_with_font_size(&self, size: f64) -> CTFont {
        unsafe {
            let font_ref = CTFontCreateCopyWithAttributes(self.0,
                                                          size as CGFloat,
                                                          ptr::null(),
                                                          ptr::null());
            CTFont::wrap_under_create_rule(font_ref)
        }
    }

    // Names
    pub fn family_name(&self) -> String {
        unsafe {
            let value = get_string_by_name_key(self, kCTFontFamilyNameKey);
            value.expect("Fonts should always have a family name.")
        }
    }

    pub fn face_name(&self) -> String {
        unsafe {
            let value = get_string_by_name_key(self, kCTFontSubFamilyNameKey);
            value.expect("Fonts should always have a face name.")
        }
    }

    pub fn unique_name(&self) -> String {
        unsafe {
            let value = get_string_by_name_key(self, kCTFontUniqueNameKey);
            value.expect("Fonts should always have a unique name.")
        }
    }

    pub fn postscript_name(&self) -> String {
        unsafe {
            let value = get_string_by_name_key(self, kCTFontPostScriptNameKey);
            value.expect("Fonts should always have a PostScript name.")
        }
    }

    pub fn all_traits(&self) -> CTFontTraits {
        unsafe {
            CTFontTraits::wrap_under_create_rule(CTFontCopyTraits(self.0))
        }
    }

    // Font metrics
    pub fn ascent(&self) -> CGFloat {
        unsafe {
            CTFontGetAscent(self.0)
        }
    }

    pub fn descent(&self) -> CGFloat {
        unsafe {
            CTFontGetDescent(self.0)
        }
    }

    pub fn underline_thickness(&self) -> CGFloat {
        unsafe {
            CTFontGetUnderlineThickness(self.0)
        }
    }

    pub fn underline_position(&self) -> CGFloat {
        unsafe {
            CTFontGetUnderlinePosition(self.0)
        }
    }

    pub fn bounding_box(&self) -> CGRect {
        unsafe {
            CTFontGetBoundingBox(self.0)
        }
    }

    pub fn leading(&self) -> CGFloat {
        unsafe {
            CTFontGetLeading(self.0)
        }
    }

    pub fn units_per_em(&self) -> libc::c_uint {
        unsafe {
            CTFontGetUnitsPerEm(self.0)
        }
    }

    pub fn x_height(&self) -> CGFloat {
        unsafe {
            CTFontGetXHeight(self.0)
        }
    }

    pub fn pt_size(&self) -> CGFloat {
        unsafe {
            CTFontGetSize(self.0)
        }
    }

    pub fn get_glyphs_for_characters(&self, characters: *const UniChar, glyphs: *mut CGGlyph, count: CFIndex)
                                     -> bool {
        unsafe {
            CTFontGetGlyphsForCharacters(self.0, characters, glyphs, count)
        }
    }

    pub fn get_advances_for_glyphs(&self,
                                   orientation: CTFontOrientation,
                                   glyphs: *const CGGlyph,
                                   advances: *mut CGSize,
                                   count: CFIndex)
                                   -> f64 {
        unsafe {
            CTFontGetAdvancesForGlyphs(self.0, orientation, glyphs, advances, count) as f64
        }
    }

    pub fn get_font_table(&self, tag: u32) -> Option<CFData> {
        unsafe {
            let result = CTFontCopyTable(self.0,
                                         tag as CTFontTableTag,
                                         kCTFontTableOptionsExcludeSynthetic);
            if result.is_null() {
                None
            } else {
                Some(CFData::wrap_under_create_rule(result))
            }
        }
    }

    pub fn get_bounding_rects_for_glyphs(&self, orientation: CTFontOrientation, glyphs: &[CGGlyph])
                                         -> CGRect {
        unsafe {
            CTFontGetBoundingRectsForGlyphs(self.as_concrete_TypeRef(),
                                            orientation,
                                            glyphs.as_ptr(),
                                            ptr::null_mut(),
                                            glyphs.len() as CFIndex)
        }
    }

    pub fn draw_glyphs(&self, glyphs: &[CGGlyph], positions: &[CGPoint], context: CGContext) {
        assert!(glyphs.len() == positions.len());
        unsafe {
            CTFontDrawGlyphs(self.as_concrete_TypeRef(),
                             glyphs.as_ptr(),
                             positions.as_ptr(),
                             glyphs.len() as size_t,
                             context.as_ptr())
        }
    }

    pub fn url(&self) -> Option<CFURL> {
        unsafe {
            let result = CTFontCopyAttribute(self.0, kCTFontURLAttribute);
            if result.is_null() {
                None
            } else {
                Some(CFURL::wrap_under_create_rule(result as CFURLRef))
            }
        }
    }

    pub fn create_path_for_glyph(&self, glyph: CGGlyph, matrix: &CGAffineTransform)
                                 -> Result<CGPath, ()> {
        unsafe {
            let path = CTFontCreatePathForGlyph(self.0, glyph, matrix);
            if path.is_null() {
                Err(())
            } else {
                Ok(CGPath::from_ptr(path))
            }
        }
    }
}

// Helper methods
fn get_string_by_name_key(font: &CTFont, name_key: CFStringRef) -> Option<String> {
    unsafe {
        let result = CTFontCopyName(font.as_concrete_TypeRef(), name_key);
        if result.is_null() {
            None
        } else {
            Some(CFString::wrap_under_create_rule(result).to_string())
        }
    }
}

pub fn debug_font_names(font: &CTFont) {
    fn get_key(font: &CTFont, key: CFStringRef) -> String {
        get_string_by_name_key(font, key).unwrap()
    }

    unsafe {
        println!("kCTFontFamilyNameKey: {}", get_key(font, kCTFontFamilyNameKey));
        println!("kCTFontSubFamilyNameKey: {}", get_key(font, kCTFontSubFamilyNameKey));
        println!("kCTFontStyleNameKey: {}", get_key(font, kCTFontStyleNameKey));
        println!("kCTFontUniqueNameKey: {}", get_key(font, kCTFontUniqueNameKey));
        println!("kCTFontFullNameKey: {}", get_key(font, kCTFontFullNameKey));
        println!("kCTFontPostScriptNameKey: {}", get_key(font, kCTFontPostScriptNameKey));
    }
}

pub fn debug_font_traits(font: &CTFont) {
    let sym = font.symbolic_traits();
    println!("kCTFontItalicTrait: {}", sym.is_italic());
    println!("kCTFontBoldTrait: {}", sym.is_bold());
    println!("kCTFontExpandedTrait: {}", sym.is_expanded());
    println!("kCTFontCondensedTrait: {}", sym.is_condensed());
    println!("kCTFontMonoSpaceTrait: {}", sym.is_monospace());

    let traits = font.all_traits();
    println!("kCTFontWeightTrait: {}", traits.normalized_weight());
//    println!("kCTFontWidthTrait: {}", traits.normalized_width());
//    println!("kCTFontSlantTrait: {}", traits.normalized_slant());
}

#[cfg(feature = "mountainlion")]
pub fn cascade_list_for_languages(font: &CTFont, language_pref_list: &CFArray<CFString>) -> CFArray<CTFontDescriptor> {
    unsafe {
        let font_collection_ref =
            CTFontCopyDefaultCascadeListForLanguages(font.as_concrete_TypeRef(),
                                                     language_pref_list.as_concrete_TypeRef());
        CFArray::wrap_under_create_rule(font_collection_ref)
    }
}

#[link(name = "CoreText", kind = "framework")]
extern {
    /*
     * CTFont.h
     */

    /* Name Specifier Constants */
    //static kCTFontCopyrightNameKey: CFStringRef;
    static kCTFontFamilyNameKey: CFStringRef;
    static kCTFontSubFamilyNameKey: CFStringRef;
    static kCTFontStyleNameKey: CFStringRef;
    static kCTFontUniqueNameKey: CFStringRef;
    static kCTFontFullNameKey: CFStringRef;
    //static kCTFontVersionNameKey: CFStringRef;
    static kCTFontPostScriptNameKey: CFStringRef;
    //static kCTFontTrademarkNameKey: CFStringRef;
    //static kCTFontManufacturerNameKey: CFStringRef;
    //static kCTFontDesignerNameKey: CFStringRef;
    //static kCTFontDescriptionNameKey: CFStringRef;
    //static kCTFontVendorURLNameKey: CFStringRef;
    //static kCTFontDesignerURLNameKey: CFStringRef;
    //static kCTFontLicenseNameKey: CFStringRef;
    //static kCTFontLicenseURLNameKey: CFStringRef;
    //static kCTFontSampleTextNameKey: CFStringRef;
    //static kCTFontPostScriptCIDNameKey: CFStringRef;

    //static kCTFontVariationAxisIdentifierKey: CFStringRef;
    //static kCTFontVariationAxisMinimumValueKey: CFStringRef;
    //static kCTFontVariationAxisMaximumValueKey: CFStringRef;
    //static kCTFontVariationAxisDefaultValueKey: CFStringRef;
    //static kCTFontVariationAxisNameKey: CFStringRef;

    //static kCTFontFeatureTypeIdentifierKey: CFStringRef;
    //static kCTFontFeatureTypeNameKey: CFStringRef;
    //static kCTFontFeatureTypeExclusiveKey: CFStringRef;
    //static kCTFontFeatureTypeSelectorsKey: CFStringRef;
    //static kCTFontFeatureSelectorIdentifierKey: CFStringRef;
    //static kCTFontFeatureSelectorNameKey: CFStringRef;
    //static kCTFontFeatureSelectorDefaultKey: CFStringRef;
    //static kCTFontFeatureSelectorSettingKey: CFStringRef;

    static kCTFontURLAttribute: CFStringRef;

    // N.B. Unlike most Cocoa bindings, this extern block is organized according
    // to the documentation's Functions By Task listing, because there so many functions.

    /* Creating Fonts */
    fn CTFontCreateWithName(name: CFStringRef, size: CGFloat, matrix: *const CGAffineTransform) -> CTFontRef;
    //fn CTFontCreateWithNameAndOptions
    fn CTFontCreateWithFontDescriptor(descriptor: CTFontDescriptorRef, size: CGFloat,
                                      matrix: *const CGAffineTransform) -> CTFontRef;
    //fn CTFontCreateWithFontDescriptorAndOptions
    //fn CTFontCreateUIFontForLanguage
    fn CTFontCreateCopyWithAttributes(font: CTFontRef, size: CGFloat, matrix: *const CGAffineTransform,
                                      attributes: CTFontDescriptorRef) -> CTFontRef;
    //fn CTFontCreateCopyWithSymbolicTraits
    //fn CTFontCreateCopyWithFamily
    //fn CTFontCreateForString

    /* Getting Font Data */
    //fn CTFontCopyFontDescriptor(font: CTFontRef) -> CTFontDescriptorRef;
    fn CTFontCopyAttribute(font: CTFontRef, attribute: CFStringRef) -> CFTypeRef;
    fn CTFontGetSize(font: CTFontRef) -> CGFloat;
    //fn CTFontGetMatrix
    fn CTFontGetSymbolicTraits(font: CTFontRef) -> CTFontSymbolicTraits;
    fn CTFontCopyTraits(font: CTFontRef) -> CFDictionaryRef;

    /* Getting Font Names */
    //fn CTFontCopyPostScriptName(font: CTFontRef) -> CFStringRef;
    //fn CTFontCopyFamilyName(font: CTFontRef) -> CFStringRef;
    //fn CTFontCopyFullName(font: CTFontRef) -> CFStringRef;
    //fn CTFontCopyDisplayName(font: CTFontRef) -> CFStringRef;
    fn CTFontCopyName(font: CTFontRef, nameKey: CFStringRef) -> CFStringRef;
    //fn CTFontCopyLocalizedName(font: CTFontRef, nameKey: CFStringRef,
    //                           language: *CFStringRef) -> CFStringRef;
    #[cfg(feature = "mountainlion")]
    fn CTFontCopyDefaultCascadeListForLanguages(font: CTFontRef, languagePrefList: CFArrayRef) -> CFArrayRef;


    /* Working With Encoding */
    //fn CTFontCopyCharacterSet
    //fn CTFontGetStringEncoding
    //fn CTFontCopySupportedLanguages

    /* Getting Font Metrics */
    fn CTFontGetAscent(font: CTFontRef) -> CGFloat;
    fn CTFontGetDescent(font: CTFontRef) -> CGFloat;
    fn CTFontGetLeading(font: CTFontRef) -> CGFloat;
    fn CTFontGetUnitsPerEm(font: CTFontRef) -> libc::c_uint;
    //fn CTFontGetGlyphCount
    fn CTFontGetBoundingBox(font: CTFontRef) -> CGRect;
    fn CTFontGetUnderlinePosition(font: CTFontRef) -> CGFloat;
    fn CTFontGetUnderlineThickness(font: CTFontRef) -> CGFloat;
    //fn CTFontGetSlantAngle
    //fn CTFontGetCapHeight
    fn CTFontGetXHeight(font: CTFontRef) -> CGFloat;

    /* Getting Glyph Data */
    fn CTFontCreatePathForGlyph(font: CTFontRef, glyph: CGGlyph, matrix: *const CGAffineTransform)
                                -> CGPathRef;
    //fn CTFontGetGlyphWithName
    fn CTFontGetBoundingRectsForGlyphs(font: CTFontRef,
                                       orientation: CTFontOrientation,
                                       glyphs: *const CGGlyph,
                                       boundingRects: *mut CGRect,
                                       count: CFIndex)
                                       -> CGRect;
    fn CTFontGetAdvancesForGlyphs(font: CTFontRef, orientation: CTFontOrientation, glyphs: *const CGGlyph, advances: *mut CGSize, count: CFIndex) -> libc::c_double;
    //fn CTFontGetVerticalTranslationsForGlyphs

    /* Working With Font Variations */
    //fn CTFontCopyVariationAxes
    //fn CTFontCopyVariation

    /* Getting Font Features */
    //fn CTFontCopyFeatures
    //fn CTFontCopyFeatureSettings

    /* Working with Glyphs */
    fn CTFontGetGlyphsForCharacters(font: CTFontRef, characters: *const UniChar, glyphs: *mut CGGlyph, count: CFIndex) -> bool;
    fn CTFontDrawGlyphs(font: CTFontRef,
                        glyphs: *const CGGlyph,
                        positions: *const CGPoint,
                        count: size_t,
                        context: CGContextRef);
    //fn CTFontGetLigatureCaretPositions

    /* Converting Fonts */
    fn CTFontCopyGraphicsFont(font: CTFontRef, attributes: *mut CTFontDescriptorRef)
                              -> CGFontRef;
    fn CTFontCreateWithGraphicsFont(graphicsFont: CGFontRef, size: CGFloat,
                                    matrix: *const CGAffineTransform,
                                    attributes: CTFontDescriptorRef) -> CTFontRef;
    //fn CTFontGetPlatformFont
    //fn CTFontCreateWithPlatformFont
    //fn CTFontCreateWithQuickdrawInstance

    /* Getting Font Table Data */
    //fn CTFontCopyAvailableTables(font: CTFontRef, options: CTFontTableOptions) -> CFArrayRef;
    fn CTFontCopyTable(font: CTFontRef, table: CTFontTableTag, options: CTFontTableOptions) -> CFDataRef;

    fn CTFontGetTypeID() -> CFTypeID;
}

