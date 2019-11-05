// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(bad_style)]

extern crate backtrace_sys as bt;

use libc::uintptr_t;
use std::ffi::{CStr, OsStr};
use std::os::raw::{c_void, c_char, c_int};
use std::os::unix::prelude::*;
use std::path::Path;
use std::ptr;
use std::sync::{ONCE_INIT, Once};

use SymbolName;

pub enum Symbol {
    Syminfo {
        pc: uintptr_t,
        symname: *const c_char,
    },
    Pcinfo {
        pc: uintptr_t,
        filename: *const c_char,
        lineno: c_int,
        function: *const c_char,
    },
}

impl Symbol {
    pub fn name(&self) -> Option<SymbolName> {
        let ptr = match *self {
            Symbol::Syminfo { symname, .. } => symname,
            Symbol::Pcinfo { function, .. } => function,
        };
        if ptr.is_null() {
            None
        } else {
            Some(SymbolName::new(unsafe { CStr::from_ptr(ptr).to_bytes() }))
        }
    }

    pub fn addr(&self) -> Option<*mut c_void> {
        let pc = match *self {
            Symbol::Syminfo { pc, .. } => pc,
            Symbol::Pcinfo { pc, .. } => pc,
        };
        if pc == 0 {None} else {Some(pc as *mut _)}
    }

    pub fn filename(&self) -> Option<&Path> {
        match *self {
            Symbol::Syminfo { .. } => None,
            Symbol::Pcinfo { filename, .. } => {
                Some(Path::new(OsStr::from_bytes(unsafe {
                    CStr::from_ptr(filename).to_bytes()
                })))
            }
        }
    }

    pub fn lineno(&self) -> Option<u32> {
        match *self {
            Symbol::Syminfo { .. } => None,
            Symbol::Pcinfo { lineno, .. } => Some(lineno as u32),
        }
    }
}

extern fn error_cb(_data: *mut c_void, _msg: *const c_char,
                   _errnum: c_int) {
    // do nothing for now
}

extern fn syminfo_cb(data: *mut c_void,
                     pc: uintptr_t,
                     symname: *const c_char,
                     _symval: uintptr_t,
                     _symsize: uintptr_t) {
    unsafe {
        call(data, &super::Symbol {
            inner: Symbol::Syminfo {
                pc: pc,
                symname: symname,
            },
        });
    }
}

extern fn pcinfo_cb(data: *mut c_void,
                    pc: uintptr_t,
                    filename: *const c_char,
                    lineno: c_int,
                    function: *const c_char) -> c_int {
    unsafe {
        if filename.is_null() || function.is_null() {
            return -1
        }
        call(data, &super::Symbol {
            inner: Symbol::Pcinfo {
                pc: pc,
                filename: filename,
                lineno: lineno,
                function: function,
            },
        });
        return 0
    }
}

unsafe fn call(data: *mut c_void, sym: &super::Symbol) {
    let cb = data as *mut &mut FnMut(&super::Symbol);
    let mut bomb = ::Bomb { enabled: true };
    (*cb)(sym);
    bomb.enabled = false;
}

// The libbacktrace API supports creating a state, but it does not
// support destroying a state. I personally take this to mean that a
// state is meant to be created and then live forever.
//
// I would love to register an at_exit() handler which cleans up this
// state, but libbacktrace provides no way to do so.
//
// With these constraints, this function has a statically cached state
// that is calculated the first time this is requested. Remember that
// backtracing all happens serially (one global lock).
//
// Things don't work so well on not-Linux since libbacktrace can't track down
// that executable this is. We at one point used env::current_exe but it turns
// out that there are some serious security issues with that approach.
//
// Specifically, on certain platforms like BSDs, a malicious actor can cause an
// arbitrary file to be placed at the path returned by current_exe. libbacktrace
// does not behave defensively in the presence of ill-formed DWARF information,
// and has been demonstrated to segfault in at least one case. There is no
// evidence at the moment to suggest that a more carefully constructed file
// can't cause arbitrary code execution. As a result of all of this, we don't
// hint libbacktrace with the path to the current process.
unsafe fn init_state() -> *mut bt::backtrace_state {
    static mut STATE: *mut bt::backtrace_state = 0 as *mut _;
    static INIT: Once = ONCE_INIT;
    INIT.call_once(|| {
        // Our libbacktrace may not have multithreading support, so
        // set `threaded = 0` and synchronize ourselves.
        STATE = bt::backtrace_create_state(ptr::null(), 0, error_cb,
                                           ptr::null_mut());
    });

    STATE
}

pub fn resolve(symaddr: *mut c_void, mut cb: &mut FnMut(&super::Symbol)) {
    let _guard = ::lock::lock();

    // backtrace errors are currently swept under the rug
    unsafe {
        let state = init_state();
        if state.is_null() {
            return
        }

        let ret = bt::backtrace_pcinfo(state, symaddr as uintptr_t,
                                       pcinfo_cb, error_cb,
                                       &mut cb as *mut _ as *mut _);
        if ret != 0 {
            bt::backtrace_syminfo(state, symaddr as uintptr_t,
                                  syminfo_cb, error_cb,
                                  &mut cb as *mut _ as *mut _);
        }
    }
}
