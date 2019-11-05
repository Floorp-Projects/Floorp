// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Unwinding through the `backtrace` function provided in Unix.
//!
//! This is an alternative unwinding strategy for Unix platforms which don't
//! have support for libunwind but do have support for `backtrace`. Currently
//! there's not a whole lot of those though. This module is a relatively
//! straightforward binding of the `backtrace` API to the `Frame` API that we'd
//! like to have.

use core::ffi::c_void;
use core::mem;
use libc::c_int;

#[derive(Clone)]
pub struct Frame {
    addr: usize,
}

impl Frame {
    pub fn ip(&self) -> *mut c_void {
        self.addr as *mut c_void
    }
    pub fn symbol_address(&self) -> *mut c_void {
        self.ip()
    }
}

extern "C" {
    fn backtrace(buf: *mut *mut c_void, sz: c_int) -> c_int;
}

#[inline(always)]
pub unsafe fn trace(cb: &mut FnMut(&super::Frame) -> bool) {
    const SIZE: usize = 100;

    let mut buf: [*mut c_void; SIZE];
    let cnt;

    buf = mem::zeroed();
    cnt = backtrace(buf.as_mut_ptr(), SIZE as c_int);

    for addr in buf[..cnt as usize].iter() {
        let cx = super::Frame {
            inner: Frame {
                addr: *addr as usize,
            },
        };
        if !cb(&cx) {
            return;
        }
    }
}
