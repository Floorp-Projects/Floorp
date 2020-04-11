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
use core_foundation::base::{ToVoid, CFRelease, CFRetain, CFTypeID};
use font::{CGFont, CGGlyph};
use geometry::CGPoint;
use color::CGColor;
use path::CGPathRef;
use libc::{c_int, size_t};
use std::os::raw::c_void;

use std::cmp;
use std::ptr;
use std::slice;
use geometry::{CGAffineTransform, CGRect};
use image::CGImage;
use foreign_types::{ForeignType, ForeignTypeRef};

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGBlendMode {
    Normal = 0,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    SoftLight,
    HardLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
    // 10.5 and up:
    Clear,
    Copy,
    SourceIn,
    SourceOut,
    SourceAtop,
    DestinationOver,
    DestinationIn,
    DestinationOut,
    DestinationAtop,
    Xor,
    PlusDarker,
    PlusLighter,
}

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

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGLineCap {
    CGLineCapButt,
    CGLineCapRound,
    CGLineCapSquare,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGLineJoin {
    CGLineJoinMiter,
    CGLineJoinRound,
    CGLineJoinBevel,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum CGPathDrawingMode {
    CGPathFill,
    CGPathEOFill,
    CGPathStroke,
    CGPathFillStroke,
    CGPathEOFillStroke,
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
}

impl CGContextRef {
    pub fn flush(&self) {
        unsafe {
            CGContextFlush(self.as_ptr())
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

    pub fn clip_bounding_box(&self) -> CGRect {
        unsafe {
            CGContextGetClipBoundingBox(self.as_ptr())
        }
    }

    pub fn set_fill_color(&self, color: &CGColor) {
        unsafe {
            CGContextSetFillColorWithColor(self.as_ptr(), color.to_void());
        }
    }

    pub fn set_rgb_fill_color(&self, red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        unsafe {
            CGContextSetRGBFillColor(self.as_ptr(), red, green, blue, alpha)
        }
    }

    pub fn set_rgb_stroke_color(&self, red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        unsafe {
            CGContextSetRGBStrokeColor(self.as_ptr(), red, green, blue, alpha)
        }
    }

    pub fn set_gray_fill_color(&self, gray: CGFloat, alpha: CGFloat) {
        unsafe {
            CGContextSetGrayFillColor(self.as_ptr(), gray, alpha)
        }
    }

    pub fn set_blend_mode(&self, blend_mode: CGBlendMode) {
        unsafe {
            CGContextSetBlendMode(self.as_ptr(), blend_mode)
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

    pub fn set_line_cap(&self, cap: CGLineCap) {
        unsafe {
            CGContextSetLineCap(self.as_ptr(), cap)
        }
    }

    pub fn set_line_dash(&self, phase: CGFloat, lengths: &[CGFloat]) {
        unsafe {
            CGContextSetLineDash(self.as_ptr(), phase, lengths.as_ptr(), lengths.len())
        }
    }

    pub fn set_line_join(&self, join: CGLineJoin) {
        unsafe {
            CGContextSetLineJoin(self.as_ptr(), join)
        }
    }

    pub fn set_line_width(&self, width: CGFloat) {
        unsafe {
            CGContextSetLineWidth(self.as_ptr(), width)
        }
    }

    pub fn set_miter_limit(&self, limit: CGFloat) {
        unsafe {
            CGContextSetMiterLimit(self.as_ptr(), limit)
        }
    }

    pub fn add_path(&self, path: &CGPathRef) {
        unsafe {
            CGContextAddPath(self.as_ptr(), path.as_ptr());
        }
    }

    pub fn add_curve_to_point(&self,
                              cp1x: CGFloat,
                              cp1y: CGFloat,
                              cp2x: CGFloat,
                              cp2y: CGFloat,
                              x: CGFloat,
                              y: CGFloat) {
        unsafe {
            CGContextAddCurveToPoint(self.as_ptr(),
                                     cp1x, cp1y,
                                     cp2x, cp2y,
                                     x, y);
        }
    }

    pub fn add_quad_curve_to_point(&self,
                                   cpx: CGFloat,
                                   cpy: CGFloat,
                                   x: CGFloat,
                                   y: CGFloat) {
        unsafe {
            CGContextAddQuadCurveToPoint(self.as_ptr(),
                                         cpx, cpy,
                                         x, y);
        }
    }

    pub fn add_line_to_point(&self, x: CGFloat, y: CGFloat) {
        unsafe {
            CGContextAddLineToPoint(self.as_ptr(), x, y);
        }
    }

    pub fn begin_path(&self) {
         unsafe {
            CGContextBeginPath(self.as_ptr());
        }
    }

    pub fn close_path(&self) {
        unsafe {
            CGContextClosePath(self.as_ptr());
        }
    }

    pub fn move_to_point(&self, x: CGFloat, y: CGFloat) {
        unsafe {
            CGContextMoveToPoint(self.as_ptr(), x, y);
        }
    }

    pub fn clip(&self) {
        unsafe {
            CGContextClip(self.as_ptr());
        }
    }

    pub fn eo_clip(&self) {
        unsafe {
            CGContextEOClip(self.as_ptr());
        }
    }

    pub fn draw_path(&self, mode: CGPathDrawingMode) {
        unsafe {
            CGContextDrawPath(self.as_ptr(), mode);
        }
    }

    pub fn fill_path(&self) {
        unsafe {
            CGContextFillPath(self.as_ptr());
        }
    }

    pub fn eo_fill_path(&self) {
        unsafe {
            CGContextEOFillPath(self.as_ptr());
        }
    }

    pub fn stroke_path(&self) {
        unsafe {
            CGContextStrokePath(self.as_ptr());
        }
    }

    pub fn fill_rect(&self, rect: CGRect) {
        unsafe {
            CGContextFillRect(self.as_ptr(), rect)
        }
    }

    pub fn fill_rects(&self, rects: &[CGRect]) {
        unsafe {
            CGContextFillRects(self.as_ptr(), rects.as_ptr(), rects.len())
        }
    }

    pub fn clear_rect(&self, rect: CGRect) {
        unsafe {
            CGContextClearRect(self.as_ptr(), rect)
        }
    }

    pub fn stroke_rect(&self, rect: CGRect) {
        unsafe {
            CGContextStrokeRect(self.as_ptr(), rect)
        }
    }

    pub fn stroke_rect_with_width(&self, rect: CGRect, width: CGFloat) {
        unsafe {
            CGContextStrokeRectWithWidth(self.as_ptr(), rect, width)
        }
    }

    pub fn clip_to_rect(&self, rect: CGRect) {
        unsafe {
            CGContextClipToRect(self.as_ptr(), rect)
        }
    }

    pub fn clip_to_rects(&self, rects: &[CGRect]) {
        unsafe {
            CGContextClipToRects(self.as_ptr(), rects.as_ptr(), rects.len())
        }
    }

    pub fn replace_path_with_stroked_path(&self) {
        unsafe {
            CGContextReplacePathWithStrokedPath(self.as_ptr())
        }
    }

    pub fn fill_ellipse_in_rect(&self, rect: CGRect) {
        unsafe {
            CGContextFillEllipseInRect(self.as_ptr(), rect)
        }
    }

    pub fn stroke_ellipse_in_rect(&self, rect: CGRect) {
        unsafe {
            CGContextStrokeEllipseInRect(self.as_ptr(), rect)
        }
    }

    pub fn stroke_line_segments(&self, points: &[CGPoint]) {
        unsafe {
            CGContextStrokeLineSegments(self.as_ptr(), points.as_ptr(), points.len())
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

    pub fn save(&self) {
        unsafe {
            CGContextSaveGState(self.as_ptr());
        }
    }

    pub fn restore(&self) {
        unsafe {
            CGContextRestoreGState(self.as_ptr());
        }
    }

    pub fn translate(&self, tx: CGFloat, ty: CGFloat) {
        unsafe {
            CGContextTranslateCTM(self.as_ptr(), tx, ty);
        }
    }

    pub fn scale(&self, sx: CGFloat, sy: CGFloat) {
        unsafe {
            CGContextScaleCTM(self.as_ptr(), sx, sy);
        }
    }

    pub fn rotate(&self, angle: CGFloat) {
        unsafe {
            CGContextRotateCTM(self.as_ptr(), angle);
        }
    }

    pub fn get_ctm(&self) -> CGAffineTransform {
        unsafe {
            CGContextGetCTM(self.as_ptr())
        }
    }

    pub fn concat_ctm(&self, transform: CGAffineTransform) {
        unsafe {
            CGContextConcatCTM(self.as_ptr(), transform)
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
    ctx.set_miter_limit(4.);
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
    fn CGContextGetClipBoundingBox(c: ::sys::CGContextRef) -> CGRect;
    fn CGContextFlush(c: ::sys::CGContextRef);
    fn CGContextSetBlendMode(c: ::sys::CGContextRef, blendMode: CGBlendMode);
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
    fn CGContextSetFillColorWithColor(c: ::sys::CGContextRef, color: *const c_void);
    fn CGContextSetLineCap(c: ::sys::CGContextRef, cap: CGLineCap);
    fn CGContextSetLineDash(c: ::sys::CGContextRef, phase: CGFloat, lengths: *const CGFloat, count: size_t);
    fn CGContextSetLineJoin(c: ::sys::CGContextRef, join: CGLineJoin);
    fn CGContextSetLineWidth(c: ::sys::CGContextRef, width: CGFloat);
    fn CGContextSetMiterLimit(c: ::sys::CGContextRef, limit: CGFloat);

    fn CGContextAddPath(c: ::sys::CGContextRef, path: ::sys::CGPathRef);
    fn CGContextAddCurveToPoint(c: ::sys::CGContextRef,
                                cp1x: CGFloat,
                                cp1y: CGFloat,
                                cp2x: CGFloat,
                                cp2y: CGFloat,
                                x: CGFloat,
                                y: CGFloat);
    fn CGContextAddQuadCurveToPoint(c: ::sys::CGContextRef,
                                    cpx: CGFloat,
                                    cpy: CGFloat,
                                    x: CGFloat,
                                    y: CGFloat);
    fn CGContextAddLineToPoint(c: ::sys::CGContextRef,
                               x: CGFloat,
                               y: CGFloat);
    fn CGContextBeginPath(c: ::sys::CGContextRef);
    fn CGContextClosePath(c: ::sys::CGContextRef);
    fn CGContextMoveToPoint(c: ::sys::CGContextRef,
                            x: CGFloat,
                            y: CGFloat);
    fn CGContextDrawPath(c: ::sys::CGContextRef, mode: CGPathDrawingMode);
    fn CGContextFillPath(c: ::sys::CGContextRef);
    fn CGContextEOFillPath(c: ::sys::CGContextRef);
    fn CGContextClip(c: ::sys::CGContextRef);
    fn CGContextEOClip(c: ::sys::CGContextRef);
    fn CGContextStrokePath(c: ::sys::CGContextRef);
    fn CGContextSetRGBFillColor(context: ::sys::CGContextRef,
                                red: CGFloat,
                                green: CGFloat,
                                blue: CGFloat,
                                alpha: CGFloat);
    fn CGContextSetRGBStrokeColor(context: ::sys::CGContextRef,
                                  red: CGFloat,
                                  green: CGFloat,
                                  blue: CGFloat,
                                  alpha: CGFloat);
    fn CGContextSetGrayFillColor(context: ::sys::CGContextRef, gray: CGFloat, alpha: CGFloat);
    fn CGContextClearRect(context: ::sys::CGContextRef,
                          rect: CGRect);
    fn CGContextFillRect(context: ::sys::CGContextRef,
                         rect: CGRect);
    fn CGContextFillRects(context: ::sys::CGContextRef,
                          rects: *const CGRect,
                          count: size_t);
    fn CGContextStrokeRect(context: ::sys::CGContextRef,
                           rect: CGRect);
    fn CGContextStrokeRectWithWidth(context: ::sys::CGContextRef,
                                    rect: CGRect,
                                    width: CGFloat);
    fn CGContextClipToRect(context: ::sys::CGContextRef,
                           rect: CGRect);
    fn CGContextClipToRects(context: ::sys::CGContextRef,
                            rects: *const CGRect,
                            count: size_t);
    fn CGContextReplacePathWithStrokedPath(context: ::sys::CGContextRef);
    fn CGContextFillEllipseInRect(context: ::sys::CGContextRef,
                                  rect: CGRect);
    fn CGContextStrokeEllipseInRect(context: ::sys::CGContextRef,
                                    rect: CGRect);
    fn CGContextStrokeLineSegments(context: ::sys::CGContextRef,
                                    points: *const CGPoint,
                                    count: size_t);
    fn CGContextDrawImage(c: ::sys::CGContextRef, rect: CGRect, image: ::sys::CGImageRef);
    fn CGContextSetFont(c: ::sys::CGContextRef, font: ::sys::CGFontRef);
    fn CGContextSetFontSize(c: ::sys::CGContextRef, size: CGFloat);
    fn CGContextSetTextMatrix(c: ::sys::CGContextRef, t: CGAffineTransform);
    fn CGContextShowGlyphsAtPositions(c: ::sys::CGContextRef,
                                      glyphs: *const CGGlyph,
                                      positions: *const CGPoint,
                                      count: size_t);

    fn CGContextSaveGState(c: ::sys::CGContextRef);
    fn CGContextRestoreGState(c: ::sys::CGContextRef);
    fn CGContextTranslateCTM(c: ::sys::CGContextRef, tx: CGFloat, ty: CGFloat);
    fn CGContextScaleCTM(c: ::sys::CGContextRef, sx: CGFloat, sy: CGFloat);
    fn CGContextRotateCTM(c: ::sys::CGContextRef, angle: CGFloat);
    fn CGContextGetCTM(c: ::sys::CGContextRef) -> CGAffineTransform;
    fn CGContextConcatCTM(c: ::sys::CGContextRef, transform: CGAffineTransform);
}

