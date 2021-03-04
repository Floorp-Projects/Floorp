// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use cocoa_foundation::foundation::NSUInteger;

pub enum MTLHeap {}

foreign_obj_type! {
    type CType = MTLHeap;
    pub struct Heap;
    pub struct HeapRef;
}

impl HeapRef {
    pub fn cpu_cache_mode(&self) -> MTLCPUCacheMode {
        unsafe { msg_send![self, cpuCacheMode] }
    }

    pub fn storage_mode(&self) -> MTLStorageMode {
        unsafe { msg_send![self, storageMode] }
    }

    pub fn set_purgeable_state(&self, state: MTLPurgeableState) -> MTLPurgeableState {
        unsafe { msg_send![self, setPurgeableState: state] }
    }

    pub fn size(&self) -> NSUInteger {
        unsafe { msg_send![self, size] }
    }

    pub fn used_size(&self) -> NSUInteger {
        unsafe { msg_send![self, usedSize] }
    }

    pub fn max_available_size(&self, alignment: NSUInteger) -> NSUInteger {
        unsafe { msg_send![self, maxAvailableSizeWithAlignment: alignment] }
    }

    pub fn new_buffer(&self, length: u64, options: MTLResourceOptions) -> Option<Buffer> {
        unsafe {
            let ptr: *mut MTLBuffer = msg_send![self, newBufferWithLength:length
                                                                  options:options];
            if !ptr.is_null() {
                Some(Buffer::from_ptr(ptr))
            } else {
                None
            }
        }
    }

    pub fn new_texture(&self, descriptor: &TextureDescriptorRef) -> Option<Texture> {
        unsafe {
            let ptr: *mut MTLTexture = msg_send![self, newTextureWithDescriptor: descriptor];
            if !ptr.is_null() {
                Some(Texture::from_ptr(ptr))
            } else {
                None
            }
        }
    }
}

pub enum MTLHeapDescriptor {}

foreign_obj_type! {
    type CType = MTLHeapDescriptor;
    pub struct HeapDescriptor;
    pub struct HeapDescriptorRef;
}

impl HeapDescriptor {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLHeapDescriptor);
            msg_send![class, new]
        }
    }
}

impl HeapDescriptorRef {
    pub fn cpu_cache_mode(&self) -> MTLCPUCacheMode {
        unsafe { msg_send![self, cpuCacheMode] }
    }

    pub fn set_cpu_cache_mode(&self, mode: MTLCPUCacheMode) {
        unsafe { msg_send![self, setCpuCacheMode: mode] }
    }

    pub fn storage_mode(&self) -> MTLStorageMode {
        unsafe { msg_send![self, storageMode] }
    }

    pub fn set_storage_mode(&self, mode: MTLStorageMode) {
        unsafe { msg_send![self, setStorageMode: mode] }
    }

    pub fn size(&self) -> NSUInteger {
        unsafe { msg_send![self, size] }
    }

    pub fn set_size(&self, size: NSUInteger) {
        unsafe { msg_send![self, setSize: size] }
    }
}
