// Copyright © 2015, Peter Atashian
// Licensed under the MIT License <LICENSE.md>
//! FFI bindings to dwrite.
#![cfg(windows)]
extern crate winapi;
use winapi::*;
extern "system" {
    pub fn DWriteCreateFactory(
        factoryType: DWRITE_FACTORY_TYPE, iid: REFIID, factory: *mut *mut IUnknown,
    ) -> HRESULT;
}
