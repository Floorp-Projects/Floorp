// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Symbolication strategy that's OSX-specific and uses the `CoreSymbolication`
//! framework, if possible.
//!
//! This strategy uses internal private APIs that are somewhat undocumented but
//! seem to be widely used on OSX. This is the default symbolication strategy
//! for OSX, but is turned off in release builds for iOS due to reports of apps
//! being rejected due to using these APIs.
//!
//! This would probably be good to get official one day and not using private
//! APIs, but for now it should largely suffice.
//!
//! Note that this module will dynamically load `CoreSymbolication` and its APIs
//! through dlopen/dlsym, and if the loading fails this falls back to `dladdr`
//! as a symbolication strategy.

#![allow(bad_style)]

use crate::symbolize::dladdr;
use crate::symbolize::ResolveWhat;
use crate::types::BytesOrWideString;
use crate::SymbolName;
use core::ffi::c_void;
use core::mem;
use core::ptr;
use core::slice;
use libc::{c_char, c_int};

#[repr(C)]
#[derive(Copy, Clone, PartialEq)]
pub struct CSTypeRef {
    cpp_data: *const c_void,
    cpp_obj: *const c_void,
}

const CS_NOW: u64 = 0x80000000;
const CSREF_NULL: CSTypeRef = CSTypeRef {
    cpp_data: 0 as *const c_void,
    cpp_obj: 0 as *const c_void,
};

pub enum Symbol<'a> {
    Core {
        path: *const c_char,
        lineno: u32,
        name: *const c_char,
        addr: *mut c_void,
    },
    Dladdr(dladdr::Symbol<'a>),
}

impl Symbol<'_> {
    pub fn name(&self) -> Option<SymbolName> {
        let name = match *self {
            Symbol::Core { name, .. } => name,
            Symbol::Dladdr(ref info) => return info.name(),
        };
        if name.is_null() {
            None
        } else {
            Some(SymbolName::new(unsafe {
                let len = libc::strlen(name);
                slice::from_raw_parts(name as *const u8, len)
            }))
        }
    }

    pub fn addr(&self) -> Option<*mut c_void> {
        match *self {
            Symbol::Core { addr, .. } => Some(addr),
            Symbol::Dladdr(ref info) => info.addr(),
        }
    }

    fn filename_bytes(&self) -> Option<&[u8]> {
        match *self {
            Symbol::Core { path, .. } => {
                if path.is_null() {
                    None
                } else {
                    Some(unsafe {
                        let len = libc::strlen(path);
                        slice::from_raw_parts(path as *const u8, len)
                    })
                }
            }
            Symbol::Dladdr(_) => None,
        }
    }

    pub fn filename_raw(&self) -> Option<BytesOrWideString> {
        self.filename_bytes().map(BytesOrWideString::Bytes)
    }

    #[cfg(feature = "std")]
    pub fn filename(&self) -> Option<&::std::path::Path> {
        use std::ffi::OsStr;
        use std::os::unix::prelude::*;
        use std::path::Path;

        self.filename_bytes().map(OsStr::from_bytes).map(Path::new)
    }

    pub fn lineno(&self) -> Option<u32> {
        match *self {
            Symbol::Core { lineno: 0, .. } => None,
            Symbol::Core { lineno, .. } => Some(lineno),
            Symbol::Dladdr(_) => None,
        }
    }
}

macro_rules! coresymbolication {
    (#[load_path = $path:tt] extern "C" {
        $(fn $name:ident($($arg:ident: $argty:ty),*) -> $ret: ty;)*
    }) => (
        pub struct CoreSymbolication {
            // The loaded dynamic library
            dll: *mut c_void,

            // Each function pointer for each function we might use
            $($name: usize,)*
        }

        static mut CORESYMBOLICATION: CoreSymbolication = CoreSymbolication {
            // Initially we haven't loaded the dynamic library
            dll: 0 as *mut _,
            // Initiall all functions are set to zero to say they need to be
            // dynamically loaded.
            $($name: 0,)*
        };

        // Convenience typedef for each function type.
        $(pub type $name = unsafe extern "C" fn($($argty),*) -> $ret;)*

        impl CoreSymbolication {
            /// Attempts to open `dbghelp.dll`. Returns `true` if it works or
            /// `false` if `dlopen` fails.
            fn open(&mut self) -> bool {
                if !self.dll.is_null() {
                    return true;
                }
                let lib = concat!($path, "\0").as_bytes();
                unsafe {
                    self.dll = libc::dlopen(lib.as_ptr() as *const _, libc::RTLD_LAZY);
                    !self.dll.is_null()
                }
            }

            // Function for each method we'd like to use. When called it will
            // either read the cached function pointer or load it and return the
            // loaded value. Loads are asserted to succeed.
            $(pub fn $name(&mut self) -> $name {
                unsafe {
                    if self.$name == 0 {
                        let name = concat!(stringify!($name), "\0");
                        self.$name = self.symbol(name.as_bytes())
                            .expect(concat!("symbol ", stringify!($name), " is missing"));
                    }
                    mem::transmute::<usize, $name>(self.$name)
                }
            })*

            fn symbol(&self, symbol: &[u8]) -> Option<usize> {
                unsafe {
                    match libc::dlsym(self.dll, symbol.as_ptr() as *const _) as usize {
                        0 => None,
                        n => Some(n),
                    }
                }
            }
        }
    )
}

coresymbolication! {
    #[load_path = "/System/Library/PrivateFrameworks/CoreSymbolication.framework\
                 /Versions/A/CoreSymbolication"]
    extern "C" {
        fn CSSymbolicatorCreateWithPid(pid: c_int) -> CSTypeRef;
        fn CSRelease(rf: CSTypeRef) -> ();
        fn CSSymbolicatorGetSymbolWithAddressAtTime(
            cs: CSTypeRef, addr: *const c_void, time: u64) -> CSTypeRef;
        fn CSSymbolicatorGetSourceInfoWithAddressAtTime(
            cs: CSTypeRef, addr: *const c_void, time: u64) -> CSTypeRef;
        fn CSSourceInfoGetLineNumber(info: CSTypeRef) -> c_int;
        fn CSSourceInfoGetPath(info: CSTypeRef) -> *const c_char;
        fn CSSourceInfoGetSymbol(info: CSTypeRef) -> CSTypeRef;
        fn CSSymbolGetMangledName(sym: CSTypeRef) -> *const c_char;
        fn CSSymbolGetSymbolOwner(sym: CSTypeRef) -> CSTypeRef;
        fn CSSymbolOwnerGetBaseAddress(symowner: CSTypeRef) -> *mut c_void;
    }
}

unsafe fn try_resolve(addr: *mut c_void, cb: &mut FnMut(&super::Symbol)) -> bool {
    // Note that this is externally synchronized so there's no need for
    // synchronization here, making this `static mut` safer.
    let lib = &mut CORESYMBOLICATION;
    if !lib.open() {
        return false;
    }

    let cs = lib.CSSymbolicatorCreateWithPid()(libc::getpid());
    if cs == CSREF_NULL {
        return false;
    }
    let _dtor = OwnedCSTypeRef {
        ptr: cs,
        CSRelease: lib.CSRelease(),
    };

    let info = lib.CSSymbolicatorGetSourceInfoWithAddressAtTime()(cs, addr, CS_NOW);
    let sym = if info == CSREF_NULL {
        lib.CSSymbolicatorGetSymbolWithAddressAtTime()(cs, addr, CS_NOW)
    } else {
        lib.CSSourceInfoGetSymbol()(info)
    };
    if sym == CSREF_NULL {
        return false;
    }
    let owner = lib.CSSymbolGetSymbolOwner()(sym);
    if owner == CSREF_NULL {
        return false;
    }

    cb(&super::Symbol {
        inner: Symbol::Core {
            path: if info != CSREF_NULL {
                lib.CSSourceInfoGetPath()(info)
            } else {
                ptr::null()
            },
            lineno: if info != CSREF_NULL {
                lib.CSSourceInfoGetLineNumber()(info) as u32
            } else {
                0
            },
            name: lib.CSSymbolGetMangledName()(sym),
            addr: lib.CSSymbolOwnerGetBaseAddress()(owner),
        },
    });
    true
}

struct OwnedCSTypeRef {
    ptr: CSTypeRef,
    CSRelease: unsafe extern "C" fn(CSTypeRef),
}

impl Drop for OwnedCSTypeRef {
    fn drop(&mut self) {
        unsafe {
            (self.CSRelease)(self.ptr);
        }
    }
}

pub unsafe fn resolve(what: ResolveWhat, cb: &mut FnMut(&super::Symbol)) {
    let addr = what.address_or_ip();
    if try_resolve(addr, cb) {
        return;
    }
    dladdr::resolve(addr, &mut |sym| {
        cb(&super::Symbol {
            inner: Symbol::Dladdr(sym),
        })
    })
}
