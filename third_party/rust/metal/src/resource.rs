// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::DeviceRef;
use cocoa_foundation::foundation::NSUInteger;

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLPurgeableState {
    KeepCurrent = 1,
    NonVolatile = 2,
    Volatile = 3,
    Empty = 4,
}

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLCPUCacheMode {
    DefaultCache = 0,
    WriteCombined = 1,
}

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLStorageMode {
    Shared = 0,
    Managed = 1,
    Private = 2,
    Memoryless = 3,
}

pub const MTLResourceCPUCacheModeShift: NSUInteger = 0;
pub const MTLResourceCPUCacheModeMask: NSUInteger = 0xf << MTLResourceCPUCacheModeShift;
pub const MTLResourceStorageModeShift: NSUInteger = 4;
pub const MTLResourceStorageModeMask: NSUInteger = 0xf << MTLResourceStorageModeShift;

bitflags! {
    #[allow(non_upper_case_globals)]
    pub struct MTLResourceOptions: NSUInteger {
        const CPUCacheModeDefaultCache  = (MTLCPUCacheMode::DefaultCache as NSUInteger) << MTLResourceCPUCacheModeShift;
        const CPUCacheModeWriteCombined = (MTLCPUCacheMode::WriteCombined as NSUInteger) << MTLResourceCPUCacheModeShift;

        const StorageModeShared  = (MTLStorageMode::Shared as NSUInteger)  << MTLResourceStorageModeShift;
        const StorageModeManaged = (MTLStorageMode::Managed as NSUInteger) << MTLResourceStorageModeShift;
        const StorageModePrivate = (MTLStorageMode::Private as NSUInteger) << MTLResourceStorageModeShift;
        const StorageModeMemoryless = (MTLStorageMode::Memoryless as NSUInteger) << MTLResourceStorageModeShift;
    }
}

bitflags! {
    pub struct MTLResourceUsage: NSUInteger {
        const Read   = 1 << 0;
        const Write  = 1 << 1;
        const Sample = 1 << 2;
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
#[repr(C)]
pub struct MTLSizeAndAlign {
    pub size: NSUInteger,
    pub align: NSUInteger,
}

pub enum MTLResource {}

foreign_obj_type! {
    type CType = MTLResource;
    pub struct Resource;
    pub struct ResourceRef;
}

impl ResourceRef {
    pub fn device(&self) -> &DeviceRef {
        unsafe { msg_send![self, device] }
    }

    pub fn label(&self) -> &str {
        unsafe {
            let label = msg_send![self, label];
            crate::nsstring_as_str(label)
        }
    }

    pub fn set_label(&self, label: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(label);
            let () = msg_send![self, setLabel: nslabel];
        }
    }

    pub fn cpu_cache_mode(&self) -> MTLCPUCacheMode {
        unsafe { msg_send![self, cpuCacheMode] }
    }

    pub fn storage_mode(&self) -> MTLStorageMode {
        unsafe { msg_send![self, storageMode] }
    }

    pub fn set_purgeable_state(&self, state: MTLPurgeableState) -> MTLPurgeableState {
        unsafe { msg_send![self, setPurgeableState: state] }
    }

    /// Only available on macOS 10.13+ & iOS 10.11+
    pub fn allocated_size(&self) -> NSUInteger {
        unsafe { msg_send![self, allocatedSize] }
    }
}
