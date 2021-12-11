/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::UnsafeCell;
use std::mem::{self, zeroed};
use std::ptr;
use std::slice;
use winapi::ctypes::c_void;
use winapi::shared::minwindef::{BOOL, FALSE, TRUE};
use winapi::shared::winerror::S_OK;
use winapi::um::dcommon::DWRITE_MEASURING_MODE;
use winapi::um::dwrite::IDWriteRenderingParams;
use winapi::um::dwrite::DWRITE_FONT_FACE_TYPE_TRUETYPE;
use winapi::um::dwrite::{IDWriteFontFace, IDWriteFontFile};
use winapi::um::dwrite::{DWRITE_FONT_FACE_TYPE_BITMAP, DWRITE_FONT_FACE_TYPE_CFF};
use winapi::um::dwrite::{DWRITE_FONT_FACE_TYPE_RAW_CFF, DWRITE_FONT_FACE_TYPE_TYPE1};
use winapi::um::dwrite::{DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, DWRITE_FONT_FACE_TYPE_VECTOR};
use winapi::um::dwrite::{DWRITE_FONT_SIMULATIONS, DWRITE_GLYPH_METRICS};
use winapi::um::dwrite::{DWRITE_GLYPH_OFFSET, DWRITE_MATRIX, DWRITE_RENDERING_MODE};
use winapi::um::dwrite::{DWRITE_RENDERING_MODE_DEFAULT, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC};
use winapi::um::dwrite_1::IDWriteFontFace1;
use winapi::um::dwrite_3::{IDWriteFontFace5, IDWriteFontResource, DWRITE_FONT_AXIS_VALUE};
use wio::com::ComPtr;

use super::{DWriteFactory, DefaultDWriteRenderParams, FontFile, FontMetrics};
use crate::com_helpers::Com;
use crate::geometry_sink_impl::GeometrySinkImpl;
use crate::outline_builder::OutlineBuilder;

pub struct FontFace {
    native: UnsafeCell<ComPtr<IDWriteFontFace>>,
    face5: UnsafeCell<Option<ComPtr<IDWriteFontFace5>>>,
}

impl FontFace {
    pub fn take(native: ComPtr<IDWriteFontFace>) -> FontFace {
        let cell = UnsafeCell::new(native);
        FontFace {
            native: cell,
            face5: UnsafeCell::new(None),
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut IDWriteFontFace {
        (*self.native.get()).as_raw()
    }

    unsafe fn get_raw_files(&self) -> Vec<*mut IDWriteFontFile> {
        let mut number_of_files: u32 = 0;
        let hr = (*self.native.get()).GetFiles(&mut number_of_files, ptr::null_mut());
        assert!(hr == 0);

        let mut file_ptrs: Vec<*mut IDWriteFontFile> =
            vec![ptr::null_mut(); number_of_files as usize];
        let hr = (*self.native.get()).GetFiles(&mut number_of_files, file_ptrs.as_mut_ptr());
        assert!(hr == 0);
        file_ptrs
    }

    pub fn get_files(&self) -> Vec<FontFile> {
        unsafe {
            let file_ptrs = self.get_raw_files();
            file_ptrs
                .iter()
                .map(|p| FontFile::take(ComPtr::from_raw(*p)))
                .collect()
        }
    }

    pub fn create_font_face_with_simulations(
        &self,
        simulations: DWRITE_FONT_SIMULATIONS,
    ) -> FontFace {
        unsafe {
            let file_ptrs = self.get_raw_files();
            let face_type = (*self.native.get()).GetType();
            let face_index = (*self.native.get()).GetIndex();
            let mut face: *mut IDWriteFontFace = ptr::null_mut();
            let hr = (*DWriteFactory()).CreateFontFace(
                face_type,
                file_ptrs.len() as u32,
                file_ptrs.as_ptr(),
                face_index,
                simulations,
                &mut face,
            );
            for p in file_ptrs {
                let _ = ComPtr::<IDWriteFontFile>::from_raw(p);
            }
            assert!(hr == 0);
            FontFace::take(ComPtr::from_raw(face))
        }
    }

    pub fn get_glyph_count(&self) -> u16 {
        unsafe { (*self.native.get()).GetGlyphCount() }
    }

    pub fn metrics(&self) -> FontMetrics {
        unsafe {
            let font_1: Option<ComPtr<IDWriteFontFace1>> = (*self.native.get()).cast().ok();
            match font_1 {
                None => {
                    let mut metrics = mem::zeroed();
                    (*self.native.get()).GetMetrics(&mut metrics);
                    FontMetrics::Metrics0(metrics)
                }
                Some(font_1) => {
                    let mut metrics_1 = mem::zeroed();
                    font_1.GetMetrics(&mut metrics_1);
                    FontMetrics::Metrics1(metrics_1)
                }
            }
        }
    }

    pub fn get_glyph_indices(&self, code_points: &[u32]) -> Vec<u16> {
        unsafe {
            let mut glyph_indices: Vec<u16> = vec![0; code_points.len()];
            let hr = (*self.native.get()).GetGlyphIndices(
                code_points.as_ptr(),
                code_points.len() as u32,
                glyph_indices.as_mut_ptr(),
            );
            assert!(hr == 0);
            glyph_indices
        }
    }

    pub fn get_design_glyph_metrics(
        &self,
        glyph_indices: &[u16],
        is_sideways: bool,
    ) -> Vec<DWRITE_GLYPH_METRICS> {
        unsafe {
            let mut metrics: Vec<DWRITE_GLYPH_METRICS> = vec![zeroed(); glyph_indices.len()];
            let hr = (*self.native.get()).GetDesignGlyphMetrics(
                glyph_indices.as_ptr(),
                glyph_indices.len() as u32,
                metrics.as_mut_ptr(),
                is_sideways as BOOL,
            );
            assert!(hr == 0);
            metrics
        }
    }

    pub fn get_gdi_compatible_glyph_metrics(
        &self,
        em_size: f32,
        pixels_per_dip: f32,
        transform: *const DWRITE_MATRIX,
        use_gdi_natural: bool,
        glyph_indices: &[u16],
        is_sideways: bool,
    ) -> Vec<DWRITE_GLYPH_METRICS> {
        unsafe {
            let mut metrics: Vec<DWRITE_GLYPH_METRICS> = vec![zeroed(); glyph_indices.len()];
            let hr = (*self.native.get()).GetGdiCompatibleGlyphMetrics(
                em_size,
                pixels_per_dip,
                transform,
                use_gdi_natural as BOOL,
                glyph_indices.as_ptr(),
                glyph_indices.len() as u32,
                metrics.as_mut_ptr(),
                is_sideways as BOOL,
            );
            assert!(hr == 0);
            metrics
        }
    }

    /// Returns the contents of the OpenType table with the given tag.
    ///
    /// NB: The bytes of the tag are reversed! You probably want to use the `u32::swap_bytes()`
    /// method on the tag value before calling this method.
    pub fn get_font_table(&self, opentype_table_tag: u32) -> Option<Vec<u8>> {
        unsafe {
            let mut table_data_ptr: *const u8 = ptr::null_mut();
            let mut table_size: u32 = 0;
            let mut table_context: *mut c_void = ptr::null_mut();
            let mut exists: BOOL = FALSE;

            let hr = (*self.native.get()).TryGetFontTable(
                opentype_table_tag,
                &mut table_data_ptr as *mut *const _ as *mut *const c_void,
                &mut table_size,
                &mut table_context,
                &mut exists,
            );
            assert!(hr == 0);

            if exists == FALSE {
                return None;
            }

            let table_bytes = slice::from_raw_parts(table_data_ptr, table_size as usize).to_vec();

            (*self.native.get()).ReleaseFontTable(table_context);

            Some(table_bytes)
        }
    }

    pub fn get_recommended_rendering_mode(
        &self,
        em_size: f32,
        pixels_per_dip: f32,
        measure_mode: DWRITE_MEASURING_MODE,
        rendering_params: *mut IDWriteRenderingParams,
    ) -> DWRITE_RENDERING_MODE {
        unsafe {
            let mut render_mode: DWRITE_RENDERING_MODE = DWRITE_RENDERING_MODE_DEFAULT;
            let hr = (*self.native.get()).GetRecommendedRenderingMode(
                em_size,
                pixels_per_dip,
                measure_mode,
                rendering_params,
                &mut render_mode,
            );

            if hr != 0 {
                return DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC;
            }

            render_mode
        }
    }

    pub fn get_recommended_rendering_mode_default_params(
        &self,
        em_size: f32,
        pixels_per_dip: f32,
        measure_mode: DWRITE_MEASURING_MODE,
    ) -> DWRITE_RENDERING_MODE {
        self.get_recommended_rendering_mode(
            em_size,
            pixels_per_dip,
            measure_mode,
            DefaultDWriteRenderParams(),
        )
    }

    pub fn get_glyph_run_outline(
        &self,
        em_size: f32,
        glyph_indices: &[u16],
        glyph_advances: Option<&[f32]>,
        glyph_offsets: Option<&[DWRITE_GLYPH_OFFSET]>,
        is_sideways: bool,
        is_right_to_left: bool,
        outline_builder: Box<dyn OutlineBuilder>,
    ) {
        unsafe {
            let glyph_advances = match glyph_advances {
                None => ptr::null(),
                Some(glyph_advances) => {
                    assert_eq!(glyph_advances.len(), glyph_indices.len());
                    glyph_advances.as_ptr()
                }
            };
            let glyph_offsets = match glyph_offsets {
                None => ptr::null(),
                Some(glyph_offsets) => {
                    assert_eq!(glyph_offsets.len(), glyph_indices.len());
                    glyph_offsets.as_ptr()
                }
            };
            let is_sideways = if is_sideways { TRUE } else { FALSE };
            let is_right_to_left = if is_right_to_left { TRUE } else { FALSE };
            let geometry_sink = GeometrySinkImpl::new(outline_builder);
            let geometry_sink = geometry_sink.into_interface();
            let hr = (*self.native.get()).GetGlyphRunOutline(
                em_size,
                glyph_indices.as_ptr(),
                glyph_advances,
                glyph_offsets,
                glyph_indices.len() as u32,
                is_sideways,
                is_right_to_left,
                geometry_sink,
            );
            assert_eq!(hr, S_OK);
        }
    }

    #[inline]
    pub fn get_type(&self) -> FontFaceType {
        unsafe {
            match (*self.native.get()).GetType() {
                DWRITE_FONT_FACE_TYPE_CFF => FontFaceType::Cff,
                DWRITE_FONT_FACE_TYPE_RAW_CFF => FontFaceType::RawCff,
                DWRITE_FONT_FACE_TYPE_TRUETYPE => FontFaceType::TrueType,
                DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION => FontFaceType::TrueTypeCollection,
                DWRITE_FONT_FACE_TYPE_TYPE1 => FontFaceType::Type1,
                DWRITE_FONT_FACE_TYPE_VECTOR => FontFaceType::Vector,
                DWRITE_FONT_FACE_TYPE_BITMAP => FontFaceType::Bitmap,
                _ => FontFaceType::Unknown,
            }
        }
    }

    #[inline]
    pub fn get_index(&self) -> u32 {
        unsafe { (*self.native.get()).GetIndex() }
    }

    #[inline]
    unsafe fn get_face5(&self) -> Option<ComPtr<IDWriteFontFace5>> {
        if (*self.face5.get()).is_none() {
            *self.face5.get() = (*self.native.get()).cast().ok()
        }
        (*self.face5.get()).clone()
    }

    pub fn has_variations(&self) -> bool {
        unsafe {
            match self.get_face5() {
                Some(face5) => face5.HasVariations() == TRUE,
                None => false,
            }
        }
    }

    pub fn create_font_face_with_variations(
        &self,
        simulations: DWRITE_FONT_SIMULATIONS,
        axis_values: &[DWRITE_FONT_AXIS_VALUE],
    ) -> Option<FontFace> {
        unsafe {
            if let Some(face5) = self.get_face5() {
                let mut resource: *mut IDWriteFontResource = ptr::null_mut();
                let hr = face5.GetFontResource(&mut resource);
                if hr == S_OK && !resource.is_null() {
                    let resource = ComPtr::from_raw(resource);
                    let mut var_face: *mut IDWriteFontFace5 = ptr::null_mut();
                    let hr = resource.CreateFontFace(
                        simulations,
                        axis_values.as_ptr(),
                        axis_values.len() as u32,
                        &mut var_face,
                    );
                    if hr == S_OK && !var_face.is_null() {
                        let var_face = ComPtr::from_raw(var_face).cast().unwrap();
                        return Some(FontFace::take(var_face));
                    }
                }
            }
            None
        }
    }
}

impl Clone for FontFace {
    fn clone(&self) -> FontFace {
        unsafe {
            FontFace {
                native: UnsafeCell::new((*self.native.get()).clone()),
                face5: UnsafeCell::new(None),
            }
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum FontFaceType {
    Unknown,
    Cff,
    RawCff,
    TrueType,
    TrueTypeCollection,
    Type1,
    Vector,
    Bitmap,
}
