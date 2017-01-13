/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ptr;
use std::cell::UnsafeCell;

use comptr::ComPtr;
use winapi;
use std::mem;
use super::DWriteFactory;

#[derive(Debug)]
pub struct GlyphRunAnalysis {
    native: UnsafeCell<ComPtr<winapi::IDWriteGlyphRunAnalysis>>,
}

impl GlyphRunAnalysis {
    pub fn create(glyph_run: &winapi::DWRITE_GLYPH_RUN,
                  pixels_per_dip: f32,
                  transform: Option<winapi::DWRITE_MATRIX>,
                  rendering_mode: winapi::DWRITE_RENDERING_MODE,
                  measuring_mode: winapi::DWRITE_MEASURING_MODE,
                  baseline_x: f32,
                  baseline_y: f32) -> GlyphRunAnalysis
    {
        unsafe {
            let mut native: ComPtr<winapi::IDWriteGlyphRunAnalysis> = ComPtr::new();
            let hr = (*DWriteFactory()).CreateGlyphRunAnalysis(glyph_run as *const winapi::DWRITE_GLYPH_RUN,
                                                               pixels_per_dip,
                                                               transform.as_ref().map(|x| x as *const _).unwrap_or(ptr::null()),
                                                               rendering_mode, measuring_mode,
                                                               baseline_x, baseline_y,
                                                               native.getter_addrefs());
            assert!(hr == 0);
            GlyphRunAnalysis::take(native)
        }
    }

    pub fn take(native: ComPtr<winapi::IDWriteGlyphRunAnalysis>) -> GlyphRunAnalysis {
        GlyphRunAnalysis {
            native: UnsafeCell::new(native),
        }
    }

    pub fn get_alpha_texture_bounds(&self, texture_type: winapi::DWRITE_TEXTURE_TYPE) -> winapi::RECT {
        unsafe {
            let mut rect: winapi::RECT = mem::zeroed();
            rect.left = 1234;
            rect.top = 1234;
            let hr = (*self.native.get()).GetAlphaTextureBounds(texture_type, &mut rect);
            assert!(hr == 0);
            rect
        }
    }

    pub fn create_alpha_texture(&self, texture_type: winapi::DWRITE_TEXTURE_TYPE, rect: winapi::RECT) -> Vec<u8> {
        unsafe {
            let rect_pixels = (rect.right - rect.left) * (rect.bottom - rect.top);
            let rect_bytes = rect_pixels * match texture_type {
                winapi::DWRITE_TEXTURE_ALIASED_1x1 => 1,
                winapi::DWRITE_TEXTURE_CLEARTYPE_3x1 => 3,
                _ => panic!("bad texture type specified"),
            };

            let mut out_bytes: Vec<u8> = vec![0; rect_bytes as usize];
            let hr = (*self.native.get()).CreateAlphaTexture(texture_type, &rect, out_bytes.as_mut_ptr(), out_bytes.len() as u32);
            assert!(hr == 0);
            out_bytes
        }
    }
}
