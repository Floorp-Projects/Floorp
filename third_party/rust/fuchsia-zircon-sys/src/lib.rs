// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![allow(non_camel_case_types)]

#[macro_use]
extern crate bitflags;

use std::{cmp, fmt};

pub type zx_handle_t = i32;

pub type zx_status_t = i32;

pub type zx_futex_t = isize;
pub type zx_addr_t = usize;
pub type zx_paddr_t = usize;
pub type zx_vaddr_t = usize;
pub type zx_off_t = u64;

// Auto-generated using tools/gen_status.py
pub const ZX_OK                    : zx_status_t = 0;
pub const ZX_ERR_INTERNAL          : zx_status_t = -1;
pub const ZX_ERR_NOT_SUPPORTED     : zx_status_t = -2;
pub const ZX_ERR_NO_RESOURCES      : zx_status_t = -3;
pub const ZX_ERR_NO_MEMORY         : zx_status_t = -4;
pub const ZX_ERR_CALL_FAILED       : zx_status_t = -5;
pub const ZX_ERR_INTERRUPTED_RETRY : zx_status_t = -6;
pub const ZX_ERR_INVALID_ARGS      : zx_status_t = -10;
pub const ZX_ERR_BAD_HANDLE        : zx_status_t = -11;
pub const ZX_ERR_WRONG_TYPE        : zx_status_t = -12;
pub const ZX_ERR_BAD_SYSCALL       : zx_status_t = -13;
pub const ZX_ERR_OUT_OF_RANGE      : zx_status_t = -14;
pub const ZX_ERR_BUFFER_TOO_SMALL  : zx_status_t = -15;
pub const ZX_ERR_BAD_STATE         : zx_status_t = -20;
pub const ZX_ERR_TIMED_OUT         : zx_status_t = -21;
pub const ZX_ERR_SHOULD_WAIT       : zx_status_t = -22;
pub const ZX_ERR_CANCELED          : zx_status_t = -23;
pub const ZX_ERR_PEER_CLOSED       : zx_status_t = -24;
pub const ZX_ERR_NOT_FOUND         : zx_status_t = -25;
pub const ZX_ERR_ALREADY_EXISTS    : zx_status_t = -26;
pub const ZX_ERR_ALREADY_BOUND     : zx_status_t = -27;
pub const ZX_ERR_UNAVAILABLE       : zx_status_t = -28;
pub const ZX_ERR_ACCESS_DENIED     : zx_status_t = -30;
pub const ZX_ERR_IO                : zx_status_t = -40;
pub const ZX_ERR_IO_REFUSED        : zx_status_t = -41;
pub const ZX_ERR_IO_DATA_INTEGRITY : zx_status_t = -42;
pub const ZX_ERR_IO_DATA_LOSS      : zx_status_t = -43;
pub const ZX_ERR_BAD_PATH          : zx_status_t = -50;
pub const ZX_ERR_NOT_DIR           : zx_status_t = -51;
pub const ZX_ERR_NOT_FILE          : zx_status_t = -52;
pub const ZX_ERR_FILE_BIG          : zx_status_t = -53;
pub const ZX_ERR_NO_SPACE          : zx_status_t = -54;
pub const ZX_ERR_STOP              : zx_status_t = -60;
pub const ZX_ERR_NEXT              : zx_status_t = -61;

pub type zx_time_t = u64;
pub type zx_duration_t = u64;
pub const ZX_TIME_INFINITE : zx_time_t = ::std::u64::MAX;

bitflags! {
    #[repr(C)]
    pub flags zx_signals_t: u32 {
        const ZX_SIGNAL_NONE              = 0,
        const ZX_OBJECT_SIGNAL_ALL        = 0x00ffffff,
        const ZX_USER_SIGNAL_ALL          = 0xff000000,
        const ZX_OBJECT_SIGNAL_0          = 1 << 0,
        const ZX_OBJECT_SIGNAL_1          = 1 << 1,
        const ZX_OBJECT_SIGNAL_2          = 1 << 2,
        const ZX_OBJECT_SIGNAL_3          = 1 << 3,
        const ZX_OBJECT_SIGNAL_4          = 1 << 4,
        const ZX_OBJECT_SIGNAL_5          = 1 << 5,
        const ZX_OBJECT_SIGNAL_6          = 1 << 6,
        const ZX_OBJECT_SIGNAL_7          = 1 << 7,
        const ZX_OBJECT_SIGNAL_8          = 1 << 8,
        const ZX_OBJECT_SIGNAL_9          = 1 << 9,
        const ZX_OBJECT_SIGNAL_10         = 1 << 10,
        const ZX_OBJECT_SIGNAL_11         = 1 << 11,
        const ZX_OBJECT_SIGNAL_12         = 1 << 12,
        const ZX_OBJECT_SIGNAL_13         = 1 << 13,
        const ZX_OBJECT_SIGNAL_14         = 1 << 14,
        const ZX_OBJECT_SIGNAL_15         = 1 << 15,
        const ZX_OBJECT_SIGNAL_16         = 1 << 16,
        const ZX_OBJECT_SIGNAL_17         = 1 << 17,
        const ZX_OBJECT_SIGNAL_18         = 1 << 18,
        const ZX_OBJECT_SIGNAL_19         = 1 << 19,
        const ZX_OBJECT_SIGNAL_20         = 1 << 20,
        const ZX_OBJECT_SIGNAL_21         = 1 << 21,
        const ZX_OBJECT_LAST_HANDLE       = 1 << 22,
        const ZX_OBJECT_HANDLE_CLOSED     = 1 << 23,
        const ZX_USER_SIGNAL_0            = 1 << 24,
        const ZX_USER_SIGNAL_1            = 1 << 25,
        const ZX_USER_SIGNAL_2            = 1 << 26,
        const ZX_USER_SIGNAL_3            = 1 << 27,
        const ZX_USER_SIGNAL_4            = 1 << 28,
        const ZX_USER_SIGNAL_5            = 1 << 29,
        const ZX_USER_SIGNAL_6            = 1 << 30,
        const ZX_USER_SIGNAL_7            = 1 << 31,

        const ZX_OBJECT_READABLE          = ZX_OBJECT_SIGNAL_0.bits,
        const ZX_OBJECT_WRITABLE          = ZX_OBJECT_SIGNAL_1.bits,
        const ZX_OBJECT_PEER_CLOSED       = ZX_OBJECT_SIGNAL_2.bits,

        // Cancelation (handle was closed while waiting with it)
        const ZX_SIGNAL_HANDLE_CLOSED     = ZX_OBJECT_HANDLE_CLOSED.bits,

        // Only one user-more reference (handle) to the object exists.
        const ZX_SIGNAL_LAST_HANDLE       = ZX_OBJECT_LAST_HANDLE.bits,

        // Event
        const ZX_EVENT_SIGNALED           = ZX_OBJECT_SIGNAL_3.bits,

        // EventPair
        const ZX_EPAIR_SIGNALED           = ZX_OBJECT_SIGNAL_3.bits,
        const ZX_EPAIR_CLOSED             = ZX_OBJECT_SIGNAL_2.bits,

        // Task signals (process, thread, job)
        const ZX_TASK_TERMINATED          = ZX_OBJECT_SIGNAL_3.bits,

        // Channel
        const ZX_CHANNEL_READABLE         = ZX_OBJECT_SIGNAL_0.bits,
        const ZX_CHANNEL_WRITABLE         = ZX_OBJECT_SIGNAL_1.bits,
        const ZX_CHANNEL_PEER_CLOSED      = ZX_OBJECT_SIGNAL_2.bits,

        // Socket
        const ZX_SOCKET_READABLE          = ZX_OBJECT_SIGNAL_0.bits,
        const ZX_SOCKET_WRITABLE          = ZX_OBJECT_SIGNAL_1.bits,
        const ZX_SOCKET_PEER_CLOSED       = ZX_OBJECT_SIGNAL_2.bits,

        // Port
        const ZX_PORT_READABLE            = ZX_OBJECT_READABLE.bits,

        // Resource
        const ZX_RESOURCE_DESTROYED       = ZX_OBJECT_SIGNAL_3.bits,
        const ZX_RESOURCE_READABLE        = ZX_OBJECT_READABLE.bits,
        const ZX_RESOURCE_WRITABLE        = ZX_OBJECT_WRITABLE.bits,
        const ZX_RESOURCE_CHILD_ADDED     = ZX_OBJECT_SIGNAL_4.bits,

        // Fifo
        const ZX_FIFO_READABLE            = ZX_OBJECT_READABLE.bits,
        const ZX_FIFO_WRITABLE            = ZX_OBJECT_WRITABLE.bits,
        const ZX_FIFO_PEER_CLOSED         = ZX_OBJECT_PEER_CLOSED.bits,

        // Job
        const ZX_JOB_NO_PROCESSES         = ZX_OBJECT_SIGNAL_3.bits,
        const ZX_JOB_NO_JOBS              = ZX_OBJECT_SIGNAL_4.bits,

        // Process
        const ZX_PROCESS_TERMINATED       = ZX_OBJECT_SIGNAL_3.bits,

        // Thread
        const ZX_THREAD_TERMINATED        = ZX_OBJECT_SIGNAL_3.bits,

        // Log
        const ZX_LOG_READABLE             = ZX_OBJECT_READABLE.bits,
        const ZX_LOG_WRITABLE             = ZX_OBJECT_WRITABLE.bits,

        // Timer
        const ZX_TIMER_SIGNALED           = ZX_OBJECT_SIGNAL_3.bits,
    }
}

pub type zx_size_t = usize;
pub type zx_ssize_t = isize;

bitflags! {
    #[repr(C)]
    pub flags zx_rights_t: u32 {
        const ZX_RIGHT_NONE         = 0,
        const ZX_RIGHT_DUPLICATE    = 1 << 0,
        const ZX_RIGHT_TRANSFER     = 1 << 1,
        const ZX_RIGHT_READ         = 1 << 2,
        const ZX_RIGHT_WRITE        = 1 << 3,
        const ZX_RIGHT_EXECUTE      = 1 << 4,
        const ZX_RIGHT_MAP          = 1 << 5,
        const ZX_RIGHT_GET_PROPERTY = 1 << 6,
        const ZX_RIGHT_SET_PROPERTY = 1 << 7,
        const ZX_RIGHT_DEBUG        = 1 << 8,
        const ZX_RIGHT_SAME_RIGHTS  = 1 << 31,
    }
}

// clock ids
pub const ZX_CLOCK_MONOTONIC: u32 = 0;

// Buffer size limits on the cprng syscalls
pub const ZX_CPRNG_DRAW_MAX_LEN: usize = 256;
pub const ZX_CPRNG_ADD_ENTROPY_MAX_LEN: usize = 256;

// Socket flags and limits.
pub const ZX_SOCKET_HALF_CLOSE: u32 = 1;

// VM Object opcodes
pub const ZX_VMO_OP_COMMIT: u32 = 1;
pub const ZX_VMO_OP_DECOMMIT: u32 = 2;
pub const ZX_VMO_OP_LOCK: u32 = 3;
pub const ZX_VMO_OP_UNLOCK: u32 = 4;
pub const ZX_VMO_OP_LOOKUP: u32 = 5;
pub const ZX_VMO_OP_CACHE_SYNC: u32 = 6;
pub const ZX_VMO_OP_CACHE_INVALIDATE: u32 = 7;
pub const ZX_VMO_OP_CACHE_CLEAN: u32 = 8;
pub const ZX_VMO_OP_CACHE_CLEAN_INVALIDATE: u32 = 9;

// VM Object clone flags
pub const ZX_VMO_CLONE_COPY_ON_WRITE: u32 = 1;

// Mapping flags to vmar routines
bitflags! {
    #[repr(C)]
    pub flags zx_vmar_flags_t: u32 {
    // flags to vmar routines
        const ZX_VM_FLAG_PERM_READ          = 1  << 0,
        const ZX_VM_FLAG_PERM_WRITE         = 1  << 1,
        const ZX_VM_FLAG_PERM_EXECUTE       = 1  << 2,
        const ZX_VM_FLAG_COMPACT            = 1  << 3,
        const ZX_VM_FLAG_SPECIFIC           = 1  << 4,
        const ZX_VM_FLAG_SPECIFIC_OVERWRITE = 1  << 5,
        const ZX_VM_FLAG_CAN_MAP_SPECIFIC   = 1  << 6,
        const ZX_VM_FLAG_CAN_MAP_READ       = 1  << 7,
        const ZX_VM_FLAG_CAN_MAP_WRITE      = 1  << 8,
        const ZX_VM_FLAG_CAN_MAP_EXECUTE    = 1  << 9,
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum zx_cache_policy_t {
    ZX_CACHE_POLICY_CACHED = 0,
    ZX_CACHE_POLICY_UNCACHED = 1,
    ZX_CACHE_POLICY_UNCACHED_DEVICE = 2,
    ZX_CACHE_POLICY_WRITE_COMBINING = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_wait_item_t {
    pub handle: zx_handle_t,
    pub waitfor: zx_signals_t,
    pub pending: zx_signals_t,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_waitset_result_t {
    pub cookie: u64,
    pub status: zx_status_t,
    pub observed: zx_signals_t,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_channel_call_args_t {
    pub wr_bytes: *const u8,
    pub wr_handles: *const zx_handle_t,
    pub rd_bytes: *mut u8,
    pub rd_handles: *mut zx_handle_t,
    pub wr_num_bytes: u32,
    pub wr_num_handles: u32,
    pub rd_num_bytes: u32,
    pub rd_num_handles: u32,
}

pub type zx_pci_irq_swizzle_lut_t = [[[u32; 4]; 8]; 32];

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_pci_init_arg_t {
    pub dev_pin_to_global_irq: zx_pci_irq_swizzle_lut_t,
    pub num_irqs: u32,
    pub irqs: [zx_irq_t; 32],
    pub ecam_window_count: u32,
    // Note: the ecam_windows field is actually a variable size array.
    // We use a fixed size array to match the C repr.
    pub ecam_windows: [zx_ecam_window_t; 1],
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_irq_t {
    pub global_irq: u32,
    pub level_triggered: bool,
    pub active_high: bool,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_ecam_window_t {
    pub base: u64,
    pub size: usize,
    pub bus_start: u8,
    pub bus_end: u8,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_pcie_device_info_t {
    pub vendor_id: u16,
    pub device_id: u16,
    pub base_class: u8,
    pub sub_class: u8,
    pub program_interface: u8,
    pub revision_id: u8,
    pub bus_id: u8,
    pub dev_id: u8,
    pub func_id: u8,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_pci_resource_t {
    pub type_: u32,
    pub size: usize,
    // TODO: Actually a union
    pub pio_addr: usize,
}

// TODO: Actually a union
pub type zx_rrec_t = [u8; 64];

// Ports V2
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum zx_packet_type_t {
    ZX_PKT_TYPE_USER = 0,
    ZX_PKT_TYPE_SIGNAL_ONE = 1,
    ZX_PKT_TYPE_SIGNAL_REP = 2,
}

impl Default for zx_packet_type_t {
    fn default() -> Self {
        zx_packet_type_t::ZX_PKT_TYPE_USER
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct zx_packet_signal_t {
    pub trigger: zx_signals_t,
    pub observed: zx_signals_t,
    pub count: u64,
}

pub const ZX_WAIT_ASYNC_ONCE: u32 = 0;
pub const ZX_WAIT_ASYNC_REPEATING: u32 = 1;

// Actually a union of different integer types, but this should be good enough.
pub type zx_packet_user_t = [u8; 32];

#[repr(C)]
#[derive(Debug, Default, Copy, Clone, Eq, PartialEq)]
pub struct zx_port_packet_t {
    pub key: u64,
    pub packet_type: zx_packet_type_t,
    pub status: i32,
    pub union: [u8; 32],
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_guest_io_t {
    port: u16,
    access_size: u8,
    input: bool,
    // TODO: Actually a union
    data: [u8; 4],
}

#[cfg(target_arch="aarch64")]
#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_guest_memory_t {
    addr: zx_vaddr_t,
    inst: u32,
}

pub const X86_MAX_INST_LEN: usize = 15;

#[cfg(target_arch="x86_64")]
#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_guest_memory_t {
    addr: zx_vaddr_t,
    inst_len: u8,
    inst_buf: [u8; X86_MAX_INST_LEN],
}

#[repr(u8)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum zx_guest_packet_t_type {
    ZX_GUEST_PKT_MEMORY = 1,
    ZX_GUEST_PKT_IO = 2,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union zx_guest_packet_t_union {
    // ZX_GUEST_PKT_MEMORY
    memory: zx_guest_memory_t,
    // ZX_GUEST_PKT_IO
    io: zx_guest_io_t,
}

// Note: values of this type must maintain the invariant that
// `packet_type` correctly indicates the type of `contents`.
// Failure to do so will result in unsafety.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct zx_guest_packet_t {
    packet_type: zx_guest_packet_t_type,
    contents: zx_guest_packet_t_union,
}

impl fmt::Debug for zx_guest_packet_t {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "zx_guest_packet_t {{ packet_type: {:?}, contents: ", self.packet_type)?;
        match self.packet_type {
            zx_guest_packet_t_type::ZX_GUEST_PKT_MEMORY =>
                write!(f, "zx_guest_packet_t_union {{ memory: {:?} }} }}",
                    unsafe { self.contents.memory }
                ),
            zx_guest_packet_t_type::ZX_GUEST_PKT_IO =>
                write!(f, "zx_guest_packet_t_union {{ io: {:?} }} }}",
                    unsafe { self.contents.io }
                ),
        }
    }
}

impl cmp::PartialEq for zx_guest_packet_t {
    fn eq(&self, other: &Self) -> bool {
        (self.packet_type == other.packet_type) &&
        match self.packet_type {
            zx_guest_packet_t_type::ZX_GUEST_PKT_MEMORY =>
                unsafe { self.contents.memory == other.contents.memory },
            zx_guest_packet_t_type::ZX_GUEST_PKT_IO =>
                unsafe { self.contents.io == other.contents.io },
        }
    }
}

impl cmp::Eq for zx_guest_packet_t {}

#[cfg(target_arch="x86_64")]
#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_vcpu_create_args_t {
    pub ip: zx_vaddr_t,
    pub cr3: zx_vaddr_t,
    pub apic_vmo: zx_handle_t,
}

#[cfg(not(target_arch="x86_64"))]
#[repr(C)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct zx_vcpu_create_args_t {
    pub ip: zx_vaddr_t,
}

include!("definitions.rs");