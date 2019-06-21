//! This module corresponds to `mach/_structs.h`.

use std::mem;

use message::{mach_msg_type_number_t};

#[repr(C)]
#[derive(Clone)]
#[derive(Debug)]
pub struct x86_thread_state64_t {
	__rax: u64,
	__rbx: u64,
	__rcx: u64,
	__rdx: u64,
	__rdi: u64,
	__rsi: u64,
	__rbp: u64,
	__rsp: u64,
	__r8: u64,
	__r9: u64,
	__r10: u64,
	__r11: u64,
	__r12: u64,
	__r13: u64,
	__r14: u64,
	__r15: u64,
	__rip: u64,
	__rflags: u64,
	__cs: u64,
	__fs: u64,
	__gs: u64,
}

impl x86_thread_state64_t {
    pub fn new() -> x86_thread_state64_t {
        x86_thread_state64_t {
	        __rax: 0,
	        __rbx: 0,
	        __rcx: 0,
	        __rdx: 0,
	        __rdi: 0,
	        __rsi: 0,
	        __rbp: 0,
	        __rsp: 0,
	        __r8: 0,
	        __r9: 0,
	        __r10: 0,
	        __r11: 0,
	        __r12: 0,
	        __r13: 0,
	        __r14: 0,
	        __r15: 0,
	        __rip: 0,
	        __rflags: 0,
	        __cs: 0,
	        __fs: 0,
	        __gs: 0,
        }
    }

    pub fn count() -> mach_msg_type_number_t {
        (mem::size_of::<x86_thread_state64_t>() / mem::size_of::<::libc::c_int>())
        as mach_msg_type_number_t
    }
}
