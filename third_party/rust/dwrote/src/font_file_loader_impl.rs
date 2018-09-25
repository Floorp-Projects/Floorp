#![allow(non_snake_case, non_upper_case_globals)]

use std::{mem, ptr};
use std::collections::HashMap;
use std::sync::atomic::AtomicUsize;
use std::sync::{Mutex, atomic};
use std::marker::Send;
use winapi::ctypes::c_void;
use winapi::shared::basetsd::{UINT32, UINT64};
use winapi::shared::guiddef::REFIID;
use winapi::shared::minwindef::ULONG;
use winapi::shared::winerror::{E_FAIL, E_INVALIDARG, E_NOTIMPL, S_OK};
use winapi::um::dwrite::IDWriteFontFile;
use winapi::um::dwrite::{IDWriteFontFileLoader, IDWriteFontFileLoaderVtbl};
use winapi::um::dwrite::{IDWriteFontFileStream, IDWriteFontFileStreamVtbl};
use winapi::um::unknwnbase::{IUnknown, IUnknownVtbl};
use winapi::um::winnt::HRESULT;

use super::DWriteFactory;
use comptr::ComPtr;
use com_helpers::*;

struct FontFileLoader;

DEFINE_GUID!{UuidOfIDWriteFontFileLoader, 0x727cad4e, 0xd6af, 0x4c9e, 0x8a, 0x08, 0xd6, 0x95, 0xb1, 0x1c, 0xaa, 0x49}
DEFINE_GUID!{UuidOfIDWriteFontFileStream, 0x6d4865fe, 0x0ab8, 0x4d91, 0x8f, 0x62, 0x5d, 0xd6, 0xbe, 0x34, 0xa3, 0xe0}

const FontFileLoaderVtbl: &'static IDWriteFontFileLoaderVtbl = &IDWriteFontFileLoaderVtbl {
    parent: implement_iunknown!(static IDWriteFontFileLoader, UuidOfIDWriteFontFileLoader, FontFileLoader),
    CreateStreamFromKey: {
        unsafe extern "system" fn CreateStreamFromKey(
            _This: *mut IDWriteFontFileLoader,
            fontFileReferenceKey: *const c_void,
            fontFileReferenceKeySize: UINT32,
            fontFileStream: *mut *mut IDWriteFontFileStream) -> HRESULT
        {
            if fontFileReferenceKey.is_null() || fontFileStream.is_null() {
                return E_INVALIDARG
            }
            assert!(fontFileReferenceKeySize == mem::size_of::<usize>() as UINT32);
            let key = *(fontFileReferenceKey as *const usize);
            let stream = match FONT_FILE_STREAM_MAP.lock().unwrap().get_mut(&key) {
                None => {
                    *fontFileStream = ptr::null_mut();
                    return E_FAIL
                }
                Some(file_stream) => {
                    file_stream.as_ptr()
                }
            };

            // This is an addref getter, so make sure to do that!
            (*stream).AddRef();

            *fontFileStream = stream;
            S_OK
        }
        CreateStreamFromKey
    }
};

impl Com<IDWriteFontFileLoader> for FontFileLoader {
    type Vtbl = IDWriteFontFileLoaderVtbl;
    fn vtbl() -> &'static IDWriteFontFileLoaderVtbl { FontFileLoaderVtbl }
}

impl Com<IUnknown> for FontFileLoader {
    type Vtbl = IUnknownVtbl;
    fn vtbl() -> &'static IUnknownVtbl { &FontFileLoaderVtbl.parent }
}

impl FontFileLoader {
    pub fn new() -> FontFileLoader {
        FontFileLoader
    }
}

unsafe impl Send for FontFileLoader {}
unsafe impl Sync for FontFileLoader {}

struct FontFileStream {
    refcount: atomic::AtomicUsize,
    data: Vec<u8>,
}

const FontFileStreamVtbl: &'static IDWriteFontFileStreamVtbl = &IDWriteFontFileStreamVtbl {
    parent: implement_iunknown!(IDWriteFontFileStream, UuidOfIDWriteFontFileStream, FontFileStream),
    ReadFileFragment: {
        unsafe extern "system" fn ReadFileFragment(
            This: *mut IDWriteFontFileStream,
            fragmentStart: *mut *const c_void,
            fileOffset: UINT64,
            fragmentSize: UINT64,
            fragmentContext: *mut *mut c_void) -> HRESULT
        {
            let this = FontFileStream::from_interface(This);
            *fragmentContext = ptr::null_mut();
            if (fileOffset + fragmentSize) as usize > this.data.len() {
                return E_INVALIDARG
            }
            let index = fileOffset as usize;
            *fragmentStart = this.data[index..].as_mut_ptr() as *const c_void;
            S_OK
        }
        ReadFileFragment
    },
    ReleaseFileFragment: {
        unsafe extern "system" fn ReleaseFileFragment(
            _This: *mut IDWriteFontFileStream,
            _fragmentContext: *mut c_void)
        {
        }
        ReleaseFileFragment
    },
    GetFileSize: {
        unsafe extern "system" fn GetFileSize(
            This: *mut IDWriteFontFileStream,
            fileSize: *mut UINT64) -> HRESULT
        {
            let this = FontFileStream::from_interface(This);
            *fileSize = this.data.len() as UINT64;
            S_OK
        }
        GetFileSize
    },
    GetLastWriteTime: {
        unsafe extern "system" fn GetLastWriteTime(
            _This: *mut IDWriteFontFileStream,
            _lastWriteTime: *mut UINT64) -> HRESULT
        {
            E_NOTIMPL
        }
        GetLastWriteTime
    },
};

impl FontFileStream {
    pub fn new(data: &[u8]) -> FontFileStream {
        FontFileStream {
            refcount: AtomicUsize::new(1),
            data: data.to_vec(),
        }
    }
}

impl Com<IDWriteFontFileStream> for FontFileStream {
    type Vtbl = IDWriteFontFileStreamVtbl;
    fn vtbl() -> &'static IDWriteFontFileStreamVtbl { FontFileStreamVtbl }
}

impl Com<IUnknown> for FontFileStream {
    type Vtbl = IUnknownVtbl;
    fn vtbl() -> &'static IUnknownVtbl { &FontFileStreamVtbl.parent }
}

static mut FONT_FILE_KEY: atomic::AtomicUsize = atomic::ATOMIC_USIZE_INIT;

lazy_static! {
    static ref FONT_FILE_STREAM_MAP: Mutex<HashMap<usize, ComPtr<IDWriteFontFileStream>>> = {
        Mutex::new(HashMap::new())
    };

    static ref FONT_FILE_LOADER: Mutex<ComPtr<IDWriteFontFileLoader>> = {
        let ffl_native = FontFileLoader::new();
        let ffl = ComPtr::<IDWriteFontFileLoader>::from_ptr(ffl_native.into_interface());
        unsafe {
            let hr = (*DWriteFactory()).RegisterFontFileLoader(ffl.as_ptr());
            assert!(hr == 0);
        }
        Mutex::new(ffl)
    };
}

pub struct DataFontHelper;

impl DataFontHelper {
    pub fn register_font_data(font_data: &[u8]) -> (ComPtr<IDWriteFontFile>, usize) {
        unsafe {
            let key = FONT_FILE_KEY.fetch_add(1, atomic::Ordering::Relaxed);
            let font_file_stream_native = FontFileStream::new(font_data);
            let font_file_stream: ComPtr<IDWriteFontFileStream> =
                ComPtr::from_ptr(font_file_stream_native.into_interface());

            {
                let mut map = FONT_FILE_STREAM_MAP.lock().unwrap();
                map.insert(key, font_file_stream);
            }

            let mut font_file: ComPtr<IDWriteFontFile> = ComPtr::new();
            {
                let loader = FONT_FILE_LOADER.lock().unwrap();
                let hr = (*DWriteFactory()).CreateCustomFontFileReference(
                    mem::transmute(&key),
                    mem::size_of::<usize>() as UINT32,
                    loader.as_ptr(),
                    font_file.getter_addrefs());
                assert!(hr == S_OK);
            }

            (font_file, key)
        }
    }

    pub fn unregister_font_data(key: usize) {
        let mut map = FONT_FILE_STREAM_MAP.lock().unwrap();
        if map.remove(&key).is_none() {
            panic!("unregister_font_data: trying to unregister key that is no longer registered");
        }
    }
}
