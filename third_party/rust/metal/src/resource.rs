// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::{DeviceRef, HeapRef, NSUInteger};
use objc::runtime::{NO, YES};

#[repr(u64)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLPurgeableState {
    KeepCurrent = 1,
    NonVolatile = 2,
    Volatile = 3,
    Empty = 4,
}

#[repr(u64)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLCPUCacheMode {
    DefaultCache = 0,
    WriteCombined = 1,
}

#[repr(u64)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLStorageMode {
    Shared = 0,
    Managed = 1,
    Private = 2,
    /// Only available on macos(11.0), macCatalyst(14.0), ios(10.0)
    Memoryless = 3,
}

/// Only available on macos(10.15), ios(13.0)
#[repr(u64)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLHazardTrackingMode {
    Default = 0,
    Untracked = 1,
    Tracked = 2,
}

pub const MTLResourceCPUCacheModeShift: NSUInteger = 0;
pub const MTLResourceCPUCacheModeMask: NSUInteger = 0xf << MTLResourceCPUCacheModeShift;
pub const MTLResourceStorageModeShift: NSUInteger = 4;
pub const MTLResourceStorageModeMask: NSUInteger = 0xf << MTLResourceStorageModeShift;
pub const MTLResourceHazardTrackingModeShift: NSUInteger = 8;
pub const MTLResourceHazardTrackingModeMask: NSUInteger = 0x3 << MTLResourceHazardTrackingModeShift;

bitflags! {
    #[allow(non_upper_case_globals)]
    pub struct MTLResourceOptions: NSUInteger {
        const CPUCacheModeDefaultCache  = (MTLCPUCacheMode::DefaultCache as NSUInteger) << MTLResourceCPUCacheModeShift;
        const CPUCacheModeWriteCombined = (MTLCPUCacheMode::WriteCombined as NSUInteger) << MTLResourceCPUCacheModeShift;

        const StorageModeShared  = (MTLStorageMode::Shared as NSUInteger)  << MTLResourceStorageModeShift;
        const StorageModeManaged = (MTLStorageMode::Managed as NSUInteger) << MTLResourceStorageModeShift;
        const StorageModePrivate = (MTLStorageMode::Private as NSUInteger) << MTLResourceStorageModeShift;
        const StorageModeMemoryless = (MTLStorageMode::Memoryless as NSUInteger) << MTLResourceStorageModeShift;

        /// Only available on macos(10.13), ios(10.0)
        const HazardTrackingModeDefault = (MTLHazardTrackingMode::Default as NSUInteger) << MTLResourceHazardTrackingModeShift;
        /// Only available on macos(10.13), ios(10.0)
        const HazardTrackingModeUntracked = (MTLHazardTrackingMode::Untracked as NSUInteger) << MTLResourceHazardTrackingModeShift;
        /// Only available on macos(10.15), ios(13.0)
        const HazardTrackingModeTracked = (MTLHazardTrackingMode::Tracked as NSUInteger) << MTLResourceHazardTrackingModeShift;
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

    /// Only available on macos(10.15), ios(13.0)
    pub fn hazard_tracking_mode(&self) -> MTLHazardTrackingMode {
        unsafe { msg_send![self, hazardTrackingMode] }
    }

    /// Only available on macos(10.15), ios(13.0)
    pub fn resource_options(&self) -> MTLResourceOptions {
        unsafe { msg_send![self, resourceOptions] }
    }

    /// Only available on macos(10.13), ios(10.0)
    pub fn heap(&self) -> &HeapRef {
        unsafe { msg_send![self, heap] }
    }

    /// Only available on macos(10.15), ios(13.0)
    pub fn heap_offset(&self) -> NSUInteger {
        unsafe { msg_send![self, heapOffset] }
    }

    /// Only available on macos(10.13), ios(10.0)
    pub fn make_aliasable(&self) {
        unsafe { msg_send![self, makeAliasable] }
    }

    /// Only available on macos(10.13), ios(10.0)
    pub fn is_aliasable(&self) -> bool {
        unsafe {
            match msg_send![self, isAliasable] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }
}
