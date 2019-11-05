// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Symbolication strategy using `dladdr`
//!
//! The `dladdr` API is available on most Unix implementations but it's quite
//! basic, not handling inline frame information at all. Since it's so prevalent
//! though we have an option to use it!

use crate::symbolize::{dladdr, ResolveWhat, SymbolName};
use crate::types::BytesOrWideString;
use core::ffi::c_void;

pub struct Symbol<'a>(dladdr::Symbol<'a>);

impl Symbol<'_> {
    pub fn name(&self) -> Option<SymbolName> {
        self.0.name()
    }

    pub fn addr(&self) -> Option<*mut c_void> {
        self.0.addr()
    }

    pub fn filename_raw(&self) -> Option<BytesOrWideString> {
        self.0.filename_raw()
    }

    #[cfg(feature = "std")]
    pub fn filename(&self) -> Option<&::std::path::Path> {
        self.0.filename()
    }

    pub fn lineno(&self) -> Option<u32> {
        self.0.lineno()
    }
}

pub unsafe fn resolve(what: ResolveWhat, cb: &mut FnMut(&super::Symbol)) {
    dladdr::resolve(what.address_or_ip(), &mut |sym| {
        cb(&super::Symbol { inner: Symbol(sym) })
    });
}
