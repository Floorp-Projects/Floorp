// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Common support for resolving with `dladdr`, often used as a fallback if
//! other strategies don't work.

#![allow(dead_code)]

cfg_if::cfg_if! {
    if #[cfg(all(unix, not(target_os = "emscripten"), feature = "dladdr"))] {
        use core::ffi::c_void;
        use core::marker;
        use core::{mem, slice};
        use crate::SymbolName;
        use crate::types::BytesOrWideString;
        use libc::{self, Dl_info};

        pub struct Symbol<'a> {
            inner: Dl_info,
            _marker: marker::PhantomData<&'a i32>,
        }

        impl Symbol<'_> {
            pub fn name(&self) -> Option<SymbolName> {
                if self.inner.dli_sname.is_null() {
                    None
                } else {
                    let ptr = self.inner.dli_sname as *const u8;
                    unsafe {
                        let len = libc::strlen(self.inner.dli_sname);
                        Some(SymbolName::new(slice::from_raw_parts(ptr, len)))
                    }
                }
            }

            pub fn addr(&self) -> Option<*mut c_void> {
                Some(self.inner.dli_saddr as *mut _)
            }

            pub fn filename_raw(&self) -> Option<BytesOrWideString> {
                None
            }

            #[cfg(feature = "std")]
            pub fn filename(&self) -> Option<&::std::path::Path> {
                None
            }

            pub fn lineno(&self) -> Option<u32> {
                None
            }
        }

        pub unsafe fn resolve(addr: *mut c_void, cb: &mut FnMut(Symbol<'static>)) {
            let mut info = Symbol {
                inner: mem::zeroed(),
                _marker: marker::PhantomData,
            };
            // Skip null addresses to avoid calling into libc and having it do
            // things with the dynamic symbol table for no reason.
            if !addr.is_null() && libc::dladdr(addr as *mut _, &mut info.inner) != 0 {
                cb(info)
            }
        }
    } else {
        use core::ffi::c_void;
        use core::marker;
        use crate::symbolize::SymbolName;
        use crate::types::BytesOrWideString;

        pub struct Symbol<'a> {
            a: Void,
            _b: marker::PhantomData<&'a i32>,
        }

        enum Void {}

        impl Symbol<'_> {
            pub fn name(&self) -> Option<SymbolName> {
                match self.a {}
            }

            pub fn addr(&self) -> Option<*mut c_void> {
                match self.a {}
            }

            pub fn filename_raw(&self) -> Option<BytesOrWideString> {
                match self.a {}
            }

            #[cfg(feature = "std")]
            pub fn filename(&self) -> Option<&::std::path::Path> {
                match self.a {}
            }

            pub fn lineno(&self) -> Option<u32> {
                match self.a {}
            }
        }

        pub unsafe fn resolve(addr: *mut c_void, cb: &mut FnMut(Symbol<'static>)) {
            drop((addr, cb));
        }
    }
}
