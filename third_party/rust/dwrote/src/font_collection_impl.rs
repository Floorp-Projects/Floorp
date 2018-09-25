/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A temporary custom font collection that exists solely for the face-to-font mapping to work.

use std::mem;
use std::sync::atomic::AtomicUsize;
use winapi::ctypes::c_void;
use winapi::shared::guiddef::REFIID;
use winapi::shared::minwindef::{BOOL, FALSE, TRUE, ULONG};
use winapi::shared::winerror::{E_INVALIDARG, S_OK};
use winapi::um::dwrite::{IDWriteFactory, IDWriteFontCollectionLoader};
use winapi::um::dwrite::{IDWriteFontCollectionLoaderVtbl, IDWriteFontFile, IDWriteFontFileEnumerator};
use winapi::um::dwrite::{IDWriteFontFileEnumeratorVtbl};
use winapi::um::unknwnbase::{IUnknown, IUnknownVtbl};
use winapi::um::winnt::HRESULT;

use com_helpers::{Com, UuidOfIUnknown};
use comptr::ComPtr;
use FontFile;

DEFINE_GUID! {
    DWRITE_FONT_COLLECTION_LOADER_UUID,
    0x12345678, 0x1234, 0x5678, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0
}
DEFINE_GUID! {
    DWRITE_FONT_FILE_ENUMERATOR_UUID,
    0x12345678, 0x1234, 0x5678, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0
}

static FONT_COLLECTION_LOADER_VTBL: IDWriteFontCollectionLoaderVtbl =
        IDWriteFontCollectionLoaderVtbl {
    parent: implement_iunknown!(static IDWriteFontCollectionLoader,
                                DWRITE_FONT_COLLECTION_LOADER_UUID,
                                CustomFontCollectionLoaderImpl),
    CreateEnumeratorFromKey: CustomFontCollectionLoaderImpl_CreateEnumeratorFromKey,
};

pub struct CustomFontCollectionLoaderImpl {
    refcount: AtomicUsize,
    font_files: Vec<ComPtr<IDWriteFontFile>>,
}

impl Com<IDWriteFontCollectionLoader> for CustomFontCollectionLoaderImpl {
    type Vtbl = IDWriteFontCollectionLoaderVtbl;
    #[inline]
    fn vtbl() -> &'static IDWriteFontCollectionLoaderVtbl {
        &FONT_COLLECTION_LOADER_VTBL
    }
}

impl Com<IUnknown> for CustomFontCollectionLoaderImpl {
    type Vtbl = IUnknownVtbl;
    #[inline]
    fn vtbl() -> &'static IUnknownVtbl {
        &FONT_COLLECTION_LOADER_VTBL.parent
    }
}

impl CustomFontCollectionLoaderImpl {
    pub fn new(font_files: &[FontFile]) -> ComPtr<IDWriteFontCollectionLoader> {
        unsafe {
            ComPtr::already_addrefed(CustomFontCollectionLoaderImpl {
                refcount: AtomicUsize::new(1),
                font_files: font_files.iter().map(|file| file.as_com_ptr()).collect(),
            }.into_interface())
        }
    }
}

unsafe extern "system" fn CustomFontCollectionLoaderImpl_CreateEnumeratorFromKey(
        this: *mut IDWriteFontCollectionLoader,
        _: *mut IDWriteFactory,
        _: *const c_void,
        _: u32,
        out_enumerator: *mut *mut IDWriteFontFileEnumerator)
        -> HRESULT {
    let this = CustomFontCollectionLoaderImpl::from_interface(this);
    let enumerator = CustomFontFileEnumeratorImpl::new((*this).font_files.clone());
    let enumerator = ComPtr::<IDWriteFontFileEnumerator>::from_ptr(enumerator.into_interface());
    *out_enumerator = enumerator.as_ptr();
    mem::forget(enumerator);
    S_OK
}

struct CustomFontFileEnumeratorImpl {
    refcount: AtomicUsize,
    font_files: Vec<ComPtr<IDWriteFontFile>>,
    index: isize,
}

impl Com<IDWriteFontFileEnumerator> for CustomFontFileEnumeratorImpl {
    type Vtbl = IDWriteFontFileEnumeratorVtbl;
    #[inline]
    fn vtbl() -> &'static IDWriteFontFileEnumeratorVtbl {
        &FONT_FILE_ENUMERATOR_VTBL
    }
}

impl Com<IUnknown> for CustomFontFileEnumeratorImpl {
    type Vtbl = IUnknownVtbl;
    #[inline]
    fn vtbl() -> &'static IUnknownVtbl {
        &FONT_FILE_ENUMERATOR_VTBL.parent
    }
}

static FONT_FILE_ENUMERATOR_VTBL: IDWriteFontFileEnumeratorVtbl = IDWriteFontFileEnumeratorVtbl {
    parent: implement_iunknown!(static IDWriteFontFileEnumerator,
                                DWRITE_FONT_FILE_ENUMERATOR_UUID,
                                CustomFontFileEnumeratorImpl),
    GetCurrentFontFile: CustomFontFileEnumeratorImpl_GetCurrentFontFile,
    MoveNext: CustomFontFileEnumeratorImpl_MoveNext,
};

impl CustomFontFileEnumeratorImpl {
    pub fn new(font_files: Vec<ComPtr<IDWriteFontFile>>) -> CustomFontFileEnumeratorImpl {
        CustomFontFileEnumeratorImpl {
            refcount: AtomicUsize::new(1),
            font_files,
            index: -1,
        }
    }
}

unsafe extern "system" fn CustomFontFileEnumeratorImpl_GetCurrentFontFile(
        this: *mut IDWriteFontFileEnumerator,
        out_font_file: *mut *mut IDWriteFontFile)
        -> HRESULT {
    let this = CustomFontFileEnumeratorImpl::from_interface(this);
    if (*this).index < 0 || (*this).index >= (*this).font_files.len() as isize {
        return E_INVALIDARG
    }
    let new_font_file = (*this).font_files[(*this).index as usize].clone();
    *out_font_file = new_font_file.as_ptr();
    mem::forget(new_font_file);
    S_OK
}

unsafe extern "system" fn CustomFontFileEnumeratorImpl_MoveNext(
        this: *mut IDWriteFontFileEnumerator,
        has_current_file: *mut BOOL)
        -> HRESULT {
    let this = CustomFontFileEnumeratorImpl::from_interface(this);
    let font_file_count = (*this).font_files.len() as isize;
    if (*this).index < font_file_count {
        (*this).index += 1
    }
    *has_current_file = if (*this).index >= 0 && (*this).index < font_file_count {
        TRUE
    } else {
        FALSE
    };
    S_OK
}
