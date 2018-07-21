// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use base::CGFloat;
use color_space::CGColorSpace;
use core_foundation::base::{CFRelease, CFRetain, CFTypeID};
use font::{CGFont, CGGlyph};
use geometry::CGPoint;
use libc::{c_int, size_t};
use std::os::raw::c_void;

use std::cmp;
use std::ptr;
use std::slice;
use geometry::{CGAffineTransform, CGRect};
use image::CGImage;
use foreign_types::ForeignType;

#[repr(C)]
pub enum CGTextDrawingMode {
    CGTextFill,
    CGTextStroke,
    CGTextFillStroke,
    CGTextInvisible,
    CGTextFillClip,
    CGTextStrokeClip,
    CGTextClip
}

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGContext;
    fn drop = |cs| CFRelease(cs as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGContext;
    pub struct CGContextRef;
}

impl CGContext {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGContextGetTypeID()
        }
    }

    pub fn create_bitmap_context(data: Option<*mut c_void>,
                                 width: size_t,
                                 height: size_t,
                                 bits_per_component: size_t,
                                 bytes_per_row: size_t,
                                 space: &CGColorSpace,
                                 bitmap_info: u32)
                                 -> CGContext {
        unsafe {
            let result = CGBitmapContextCreate(data.unwrap_or(ptr::null_mut()),
                                               width,
                                               height,
                                               bits_per_component,
                                               bytes_per_row,
                                               space.as_ptr(),
                                               bitmap_info);
            assert!(!result.is_null());
            Self::from_ptr(result)
        }
    }

    pub fn data(&mut self) -> &mut [u8] {
        unsafe {
            slice::from_raw_parts_mut(
                    CGBitmapContextGetData(self.as_ptr()) as *mut u8,
                    (self.height() * self.bytes_per_row()) as usize)
        }
    }

    pub fn width(&self) -> size_t {
        unsafe {
            CGBitmapContextGetWidth(self.as_ptr())
        }
    }

    pub fn height(&self) -> size_t {
        unsafe {
            CGBitmapContextGetHeight(self.as_ptr())
        }
    }

    pub fn bytes_per_row(&self) -> size_t {
        unsafe {
            CGBitmapContextGetBytesPerRow(self.as_ptr())
        }
    }

    pub fn set_rgb_fill_color(&self, red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        unsafe {
            CGContextSetRGBFillColor(self.as_ptr(), red, green, blue, alpha)
        }
    }

    pub fn set_allows_font_smoothing(&self, allows_font_smoothing: bool) {
        unsafe {
            CGContextSetAllowsFontSmoothing(self.as_ptr(), allows_font_smoothing)
        }
    }

    pub fn set_font_smoothing_style(&self, style: i32) {
        unsafe {
            CGContextSetFontSmoothingStyle(self.as_ptr(), style as _);
        }
    }

    pub fn set_should_smooth_fonts(&self, should_smooth_fonts: bool) {
        unsafe {
            CGContextSetShouldSmoothFonts(self.as_ptr(), should_smooth_fonts)
        }
    }

    pub fn set_allows_antialiasing(&self, allows_antialiasing: bool) {
        unsafe {
            CGContextSetAllowsAntialiasing(self.as_ptr(), allows_antialiasing)
        }
    }

    pub fn set_should_antialias(&self, should_antialias: bool) {
        unsafe {
            CGContextSetShouldAntialias(self.as_ptr(), should_antialias)
        }
    }

    pub fn set_allows_font_subpixel_quantization(&self, allows_font_subpixel_quantization: bool) {
        unsafe {
            CGContextSetAllowsFontSubpixelQuantization(self.as_ptr(), allows_font_subpixel_quantization)
        }
    }

    pub fn set_should_subpixel_quantize_fonts(&self, should_subpixel_quantize_fonts: bool) {
        unsafe {
            CGContextSetShouldSubpixelQuantizeFonts(self.as_ptr(), should_subpixel_quantize_fonts)
        }
    }

    pub fn set_allows_font_subpixel_positioning(&self, allows_font_subpixel_positioning: bool) {
        unsafe {
            CGContextSetAllowsFontSubpixelPositioning(self.as_ptr(), allows_font_subpixel_positioning)
        }
    }

    pub fn set_should_subpixel_position_fonts(&self, should_subpixel_position_fonts: bool) {
        unsafe {
            CGContextSetShouldSubpixelPositionFonts(self.as_ptr(), should_subpixel_position_fonts)
        }
    }

    pub fn set_text_drawing_mode(&self, mode: CGTextDrawingMode) {
        unsafe {
            CGContextSetTextDrawingMode(self.as_ptr(), mode)
        }
    }

    pub fn fill_rect(&self, rect: CGRect) {
        unsafe {
            CGContextFillRect(self.as_ptr(), rect)
        }
    }

    pub fn draw_image(&self, rect: CGRect, image: &CGImage) {
        unsafe {
            CGContextDrawImage(self.as_ptr(), rect, image.as_ptr());
        }
    }

    pub fn create_image(&self) -> Option<CGImage> {
        let image = unsafe { CGBitmapContextCreateImage(self.as_ptr()) };
        if !image.is_null() {
            Some(unsafe { CGImage::from_ptr(image) })
        } else {
            None
        }
    }

    pub fn set_font(&self, font: &CGFont) {
        unsafe {
            CGContextSetFont(self.as_ptr(), font.as_ptr())
        }
    }

    pub fn set_font_size(&self, size: CGFloat) {
        unsafe {
            CGContextSetFontSize(self.as_ptr(), size)
        }
    }

    pub fn set_text_matrix(&self, t: &CGAffineTransform) {
        unsafe {
            CGContextSetTextMatrix(self.as_ptr(), *t)
        }
    }

    pub fn show_glyphs_at_positions(&self, glyphs: &[CGGlyph], positions: &[CGPoint]) {
        unsafe {
            let count = cmp::min(glyphs.len(), positions.len());
            CGContextShowGlyphsAtPositions(self.as_ptr(),
                                           glyphs.as_ptr(),
                                           positions.as_ptr(),
                                           count)
        }
    }
}

#[test]
fn create_bitmap_context_test() {
    use geometry::*;

    let cs = CGColorSpace::create_device_rgb();
    let ctx = CGContext::create_bitmap_context(None,
                                16, 8,
                                8, 0,
                                &cs,
                                ::base::kCGImageAlphaPremultipliedLast);
    ctx.set_rgb_fill_color(1.,0.,1.,1.);
    ctx.fill_rect(CGRect::new(&CGPoint::new(0.,0.), &CGSize::new(8.,8.)));
    let img = ctx.create_image().unwrap();
    assert_eq!(16, img.width());
    assert_eq!(8, img.height());
    assert_eq!(8, img.bits_per_component());
    assert_eq!(32, img.bits_per_pixel());
    let data = img.data();
    assert_eq!(255, data.bytes()[0]);
    assert_eq!(0, data.bytes()[1]);
    assert_eq!(255, data.bytes()[2]);
    assert_eq!(255, data.bytes()[3]);
}

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    fn CGBitmapContextCreate(data: *mut c_void,
                             width: size_t,
                             height: size_t,
                             bitsPerComponent: size_t,
                             bytesPerRow: size_t,
                             space: ::sys::CGColorSpaceRef,
                             bitmapInfo: u32)
                             -> ::sys::CGContextRef;
    fn CGBitmapContextGetData(context: ::sys::CGContextRef) -> *mut c_void;
    fn CGBitmapContextGetWidth(context: ::sys::CGContextRef) -> size_t;
    fn CGBitmapContextGetHeight(context: ::sys::CGContextRef) -> size_t;
    fn CGBitmapContextGetBytesPerRow(context: ::sys::CGContextRef) -> size_t;
    fn CGBitmapContextCreateImage(context: ::sys::CGContextRef) -> ::sys::CGImageRef;
    fn CGContextGetTypeID() -> CFTypeID;
    fn CGContextSetAllowsFontSmoothing(c: ::sys::CGContextRef, allowsFontSmoothing: bool);
    fn CGContextSetShouldSmoothFonts(c: ::sys::CGContextRef, shouldSmoothFonts: bool);
    fn CGContextSetFontSmoothingStyle(c: ::sys::CGContextRef, style: c_int);
    fn CGContextSetAllowsAntialiasing(c: ::sys::CGContextRef, allowsAntialiasing: bool);
    fn CGContextSetShouldAntialias(c: ::sys::CGContextRef, shouldAntialias: bool);
    fn CGContextSetAllowsFontSubpixelQuantization(c: ::sys::CGContextRef,
                                                  allowsFontSubpixelQuantization: bool);
    fn CGContextSetShouldSubpixelQuantizeFonts(c: ::sys::CGContextRef,
                                               shouldSubpixelQuantizeFonts: bool);
    fn CGContextSetAllowsFontSubpixelPositioning(c: ::sys::CGContextRef,
                                                 allowsFontSubpixelPositioning: bool);
    fn CGContextSetShouldSubpixelPositionFonts(c: ::sys::CGContextRef,
                                               shouldSubpixelPositionFonts: bool);
    fn CGContextSetTextDrawingMode(c: ::sys::CGContextRef, mode: CGTextDrawingMode);
    fn CGContextSetRGBFillColor(context: ::sys::CGContextRef,
                                red: CGFloat,
                                green: CGFloat,
                                blue: CGFloat,
                                alpha: CGFloat);
    fn CGContextFillRect(context: ::sys::CGContextRef,
                         rect: CGRect);
    fn CGContextDrawImage(c: ::sys::CGContextRef, rect: CGRect, image: ::sys::CGImageRef);
    fn CGContextSetFont(c: ::sys::CGContextRef, font: ::sys::CGFontRef);
    fn CGContextSetFontSize(c: ::sys::CGContextRef, size: CGFloat);
    fn CGContextSetTextMatrix(c: ::sys::CGContextRef, t: CGAffineTransform);
    fn CGContextShowGlyphsAtPositions(c: ::sys::CGContextRef,
                                      glyphs: *const CGGlyph,
                                      positions: *const CGPoint,
                                      count: size_t);
}

