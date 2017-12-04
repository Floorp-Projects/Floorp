/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_upper_case_globals)]

#[macro_use]
extern crate serde_derive;

#[macro_use]
extern crate lazy_static;
#[macro_use(DEFINE_GUID)]
extern crate winapi;
extern crate gdi32;
extern crate kernel32;
extern crate libc;
extern crate serde;

include!("types.rs");

use winapi::DWRITE_FACTORY_TYPE_SHARED;
use winapi::IDWriteFactory;
use winapi::IDWriteRenderingParams;
use std::ffi::CString;

use comptr::ComPtr;
use winapi::S_OK;

mod comptr;
mod helpers;
use helpers::ToWide;
use std::os::raw::c_void;

#[cfg(test)]
mod test;

// We still use the DWrite structs for things like metrics; re-export them
// here
pub use winapi::DWRITE_FONT_METRICS as FontMetrics;
pub use winapi::DWRITE_GLYPH_OFFSET as GlyphOffset;
pub use winapi::{DWRITE_MATRIX, DWRITE_GLYPH_RUN};
pub use winapi::{DWRITE_RENDERING_MODE_DEFAULT,
                 DWRITE_RENDERING_MODE_ALIASED,
                 DWRITE_RENDERING_MODE_GDI_CLASSIC,
                 DWRITE_RENDERING_MODE_GDI_NATURAL,
                 DWRITE_RENDERING_MODE_NATURAL,
                 DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                 DWRITE_RENDERING_MODE_OUTLINE,
                 DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC,
                 DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL,
                 DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL,
                 DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC};
pub use winapi::{DWRITE_MEASURING_MODE_NATURAL,
                 DWRITE_MEASURING_MODE_GDI_CLASSIC,
                 DWRITE_MEASURING_MODE_GDI_NATURAL};
pub use winapi::{DWRITE_FONT_SIMULATIONS_NONE,
                 DWRITE_FONT_SIMULATIONS_BOLD,
                 DWRITE_FONT_SIMULATIONS_OBLIQUE};
pub use winapi::{DWRITE_TEXTURE_ALIASED_1x1, DWRITE_TEXTURE_CLEARTYPE_3x1};
pub use winapi::{DWRITE_FONT_SIMULATIONS};
pub use winapi::{DWRITE_RENDERING_MODE};
pub use winapi::{DWRITE_MEASURING_MODE};
pub use winapi::{DWRITE_TEXTURE_TYPE};

#[macro_use] mod com_helpers;

mod bitmap_render_target; pub use bitmap_render_target::BitmapRenderTarget;
mod font; pub use font::Font;
mod font_collection; pub use font_collection::FontCollection;
mod font_face; pub use font_face::FontFace;
mod font_family; pub use font_family::FontFamily;
mod font_file; pub use font_file::FontFile;
mod gdi_interop; pub use gdi_interop::GdiInterop;
mod rendering_params; pub use rendering_params::RenderingParams;
mod glyph_run_analysis; pub use glyph_run_analysis::GlyphRunAnalysis;

// This is an internal implementation of FontFileLoader, for our utility
// functions.  We don't wrap the DWriteFontFileLoader interface and
// related things.
mod font_file_loader_impl;

DEFINE_GUID!{UuidOfIDWriteFactory, 0xb859ee5a, 0xd838, 0x4b5b, 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}

unsafe impl Sync for ComPtr<IDWriteFactory> { }
unsafe impl Sync for ComPtr<IDWriteRenderingParams> {}

lazy_static! {
    static ref DWRITE_FACTORY_RAW_PTR: usize = {
        unsafe {
            type DWriteCreateFactoryType = extern "system" fn(winapi::DWRITE_FACTORY_TYPE, winapi::REFIID, *mut *mut winapi::IUnknown) -> winapi::HRESULT;

            let dwrite_dll = kernel32::LoadLibraryW("dwrite.dll".to_wide_null().as_ptr());
            assert!(!dwrite_dll.is_null());
            let create_factory_name = CString::new("DWriteCreateFactory").unwrap();
            let dwrite_create_factory_ptr =
                kernel32::GetProcAddress(dwrite_dll, create_factory_name.as_ptr() as winapi::LPCSTR);
            assert!(!dwrite_create_factory_ptr.is_null());

            let dwrite_create_factory =
                mem::transmute::<*const c_void, DWriteCreateFactoryType>(dwrite_create_factory_ptr);

            let mut factory: ComPtr<IDWriteFactory> = ComPtr::new();
            let hr = dwrite_create_factory(
                DWRITE_FACTORY_TYPE_SHARED,
                &UuidOfIDWriteFactory,
                factory.getter_addrefs());
            assert!(hr == S_OK);
            factory.forget() as usize
        }
    };

  static ref DEFAULT_DWRITE_RENDERING_PARAMS_RAW_PTR: usize = {
    unsafe {
      let mut default_rendering_params: ComPtr<IDWriteRenderingParams> = ComPtr::new();
      let hr = (*DWriteFactory()).CreateRenderingParams(default_rendering_params.getter_addrefs());
      assert!(hr == S_OK);

      default_rendering_params.forget() as usize
    }
  };

} // end lazy static

// FIXME vlad would be nice to return, say, FactoryPtr<IDWriteFactory>
// that has a DerefMut impl, so that we can write
// DWriteFactory().SomeOperation() as opposed to
// (*DWriteFactory()).SomeOperation()
#[allow(non_snake_case)]
fn DWriteFactory() -> *mut IDWriteFactory {
    (*DWRITE_FACTORY_RAW_PTR) as *mut IDWriteFactory
}

#[allow(non_snake_case)]
fn DefaultDWriteRenderParams() -> *mut IDWriteRenderingParams {
  (*DEFAULT_DWRITE_RENDERING_PARAMS_RAW_PTR) as *mut IDWriteRenderingParams
}
