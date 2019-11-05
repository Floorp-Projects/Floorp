// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Backtrace strategy for MSVC platforms.
//!
//! This module contains the ability to generate a backtrace on MSVC using one
//! of two possible methods. The `StackWalkEx` function is primarily used if
//! possible, but not all systems have that. Failing that the `StackWalk64`
//! function is used instead. Note that `StackWalkEx` is favored because it
//! handles debuginfo internally and returns inline frame information.
//!
//! Note that all dbghelp support is loaded dynamically, see `src/dbghelp.rs`
//! for more information about that.

#![allow(bad_style)]

use core::ffi::c_void;
use core::mem;
use crate::dbghelp;
use crate::windows::*;

#[derive(Clone, Copy)]
pub enum Frame {
    New(STACKFRAME_EX),
    Old(STACKFRAME64),
}

// we're just sending around raw pointers and reading them, never interpreting
// them so this should be safe to both send and share across threads.
unsafe impl Send for Frame {}
unsafe impl Sync for Frame {}

impl Frame {
    pub fn ip(&self) -> *mut c_void {
        self.addr_pc().Offset as *mut _
    }

    pub fn symbol_address(&self) -> *mut c_void {
        self.ip()
    }

    fn addr_pc(&self) -> &ADDRESS64 {
        match self {
            Frame::New(new) => &new.AddrPC,
            Frame::Old(old) => &old.AddrPC,
        }
    }

    fn addr_pc_mut(&mut self) -> &mut ADDRESS64 {
        match self {
            Frame::New(new) => &mut new.AddrPC,
            Frame::Old(old) => &mut old.AddrPC,
        }
    }

    fn addr_frame_mut(&mut self) -> &mut ADDRESS64 {
        match self {
            Frame::New(new) => &mut new.AddrFrame,
            Frame::Old(old) => &mut old.AddrFrame,
        }
    }

    fn addr_stack_mut(&mut self) -> &mut ADDRESS64 {
        match self {
            Frame::New(new) => &mut new.AddrStack,
            Frame::Old(old) => &mut old.AddrStack,
        }
    }
}

#[repr(C, align(16))] // required by `CONTEXT`, is a FIXME in winapi right now
struct MyContext(CONTEXT);

#[inline(always)]
pub unsafe fn trace(cb: &mut FnMut(&super::Frame) -> bool) {
    // Allocate necessary structures for doing the stack walk
    let process = GetCurrentProcess();
    let thread = GetCurrentThread();

    let mut context = mem::zeroed::<MyContext>();
    RtlCaptureContext(&mut context.0);

    // Ensure this process's symbols are initialized
    let dbghelp = match dbghelp::init() {
        Ok(dbghelp) => dbghelp,
        Err(()) => return, // oh well...
    };

    // Attempt to use `StackWalkEx` if we can, but fall back to `StackWalk64`
    // since it's in theory supported on more systems.
    match (*dbghelp.dbghelp()).StackWalkEx() {
        Some(StackWalkEx) => {
            let mut frame = super::Frame {
                inner: Frame::New(mem::zeroed()),
            };
            let image = init_frame(&mut frame.inner, &context.0);
            let frame_ptr = match &mut frame.inner {
                Frame::New(ptr) => ptr as *mut STACKFRAME_EX,
                _ => unreachable!(),
            };

            while StackWalkEx(
                image as DWORD,
                process,
                thread,
                frame_ptr,
                &mut context.0 as *mut CONTEXT as *mut _,
                None,
                Some(dbghelp.SymFunctionTableAccess64()),
                Some(dbghelp.SymGetModuleBase64()),
                None,
                0,
            ) == TRUE
            {
                if !cb(&frame) {
                    break;
                }
            }
        }
        None => {
            let mut frame = super::Frame {
                inner: Frame::Old(mem::zeroed()),
            };
            let image = init_frame(&mut frame.inner, &context.0);
            let frame_ptr = match &mut frame.inner {
                Frame::Old(ptr) => ptr as *mut STACKFRAME64,
                _ => unreachable!(),
            };

            while dbghelp.StackWalk64()(
                image as DWORD,
                process,
                thread,
                frame_ptr,
                &mut context.0 as *mut CONTEXT as *mut _,
                None,
                Some(dbghelp.SymFunctionTableAccess64()),
                Some(dbghelp.SymGetModuleBase64()),
                None,
            ) == TRUE
            {
                if !cb(&frame) {
                    break;
                }
            }
        }
    }
}

#[cfg(target_arch = "x86_64")]
fn init_frame(frame: &mut Frame, ctx: &CONTEXT) -> WORD {
    frame.addr_pc_mut().Offset = ctx.Rip as u64;
    frame.addr_pc_mut().Mode = AddrModeFlat;
    frame.addr_stack_mut().Offset = ctx.Rsp as u64;
    frame.addr_stack_mut().Mode = AddrModeFlat;
    frame.addr_frame_mut().Offset = ctx.Rbp as u64;
    frame.addr_frame_mut().Mode = AddrModeFlat;

    IMAGE_FILE_MACHINE_AMD64
}

#[cfg(target_arch = "x86")]
fn init_frame(frame: &mut Frame, ctx: &CONTEXT) -> WORD {
    frame.addr_pc_mut().Offset = ctx.Eip as u64;
    frame.addr_pc_mut().Mode = AddrModeFlat;
    frame.addr_stack_mut().Offset = ctx.Esp as u64;
    frame.addr_stack_mut().Mode = AddrModeFlat;
    frame.addr_frame_mut().Offset = ctx.Ebp as u64;
    frame.addr_frame_mut().Mode = AddrModeFlat;

    IMAGE_FILE_MACHINE_I386
}

#[cfg(target_arch = "aarch64")]
fn init_frame(frame: &mut Frame, ctx: &CONTEXT) -> WORD {
    frame.addr_pc_mut().Offset = ctx.Pc as u64;
    frame.addr_pc_mut().Mode = AddrModeFlat;
    frame.addr_stack_mut().Offset = ctx.Sp as u64;
    frame.addr_stack_mut().Mode = AddrModeFlat;
    unsafe {
        frame.addr_frame_mut().Offset = ctx.u.s().Fp as u64;
    }
    frame.addr_frame_mut().Mode = AddrModeFlat;
    IMAGE_FILE_MACHINE_ARM64
}

#[cfg(target_arch = "arm")]
fn init_frame(frame: &mut Frame, ctx: &CONTEXT) -> WORD {
    frame.addr_pc_mut().Offset = ctx.Pc as u64;
    frame.addr_pc_mut().Mode = AddrModeFlat;
    frame.addr_stack_mut().Offset = ctx.Sp as u64;
    frame.addr_stack_mut().Mode = AddrModeFlat;
    unsafe {
        frame.addr_frame_mut().Offset = ctx.R11 as u64;
    }
    frame.addr_frame_mut().Mode = AddrModeFlat;
    IMAGE_FILE_MACHINE_ARMNT
}
