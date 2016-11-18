/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "serde_derive", feature(proc_macro, rustc_attrs, structural_match))]
#![allow(non_upper_case_globals)]

#[cfg(feature = "serde_derive")]
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

#[cfg(feature = "serde_codegen")]
include!(concat!(env!("OUT_DIR"), "/types.rs"));

#[cfg(feature = "serde_derive")]
include!("types.rs");

use winapi::DWRITE_FACTORY_TYPE_SHARED;
use winapi::IDWriteFactory;
use std::ffi::CString;

use comptr::ComPtr;
use winapi::S_OK;

mod comptr;
mod helpers;
use helpers::ToWide;

#[cfg(test)]
mod test;


// We still use the DWrite structs for things like metrics; re-export them
// here
pub use winapi::DWRITE_FONT_METRICS as FontMetrics;
pub use winapi::DWRITE_GLYPH_OFFSET as GlyphOffset;
pub use winapi::{DWRITE_MEASURING_MODE_NATURAL, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL};

mod bitmap_render_target; pub use bitmap_render_target::BitmapRenderTarget;
mod font; pub use font::Font;
mod font_collection; pub use font_collection::FontCollection;
mod font_face; pub use font_face::FontFace;
mod font_family; pub use font_family::FontFamily;
mod font_file; pub use font_file::FontFile;
mod gdi_interop; pub use gdi_interop::GdiInterop;
mod rendering_params; pub use rendering_params::RenderingParams;

DEFINE_GUID!{UuidOfIDWriteFactory, 0xb859ee5a, 0xd838, 0x4b5b, 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}

unsafe impl Sync for ComPtr<IDWriteFactory> { }

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

            println!("create_factory_ptr: {:?}", dwrite_create_factory_ptr);
            let dwrite_create_factory: DWriteCreateFactoryType = mem::transmute(dwrite_create_factory_ptr);

            let mut factory: ComPtr<IDWriteFactory> = ComPtr::new();
            let hr = dwrite_create_factory(
                DWRITE_FACTORY_TYPE_SHARED,
                &UuidOfIDWriteFactory,
                factory.getter_addrefs());
            assert!(hr == S_OK);
            factory.forget() as usize
        }
    };
}

// FIXME vlad would be nice to return, say, FactoryPtr<IDWriteFactory>
// that has a DerefMut impl, so that we can write
// DWriteFactory().SomeOperation() as opposed to
// (*DWriteFactory()).SomeOperation()
#[allow(non_snake_case)]
fn DWriteFactory() -> *mut IDWriteFactory {
    (*DWRITE_FACTORY_RAW_PTR) as *mut IDWriteFactory
}
