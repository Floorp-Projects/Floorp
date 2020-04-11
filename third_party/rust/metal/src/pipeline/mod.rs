// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

mod compute;
mod render;

pub use self::compute::*;
pub use self::render::*;

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLMutability {
    Default = 0,
    Mutable = 1,
    Immutable = 2,
}

impl Default for MTLMutability {
    #[inline]
    fn default() -> Self {
        MTLMutability::Default
    }
}

pub enum MTLPipelineBufferDescriptorArray {}

foreign_obj_type! {
    type CType = MTLPipelineBufferDescriptorArray;
    pub struct PipelineBufferDescriptorArray;
    pub struct PipelineBufferDescriptorArrayRef;
}

impl PipelineBufferDescriptorArrayRef {
    pub fn object_at(&self, index: NSUInteger) -> Option<&PipelineBufferDescriptorRef> {
        unsafe { msg_send![self, objectAtIndexedSubscript: index] }
    }

    pub fn set_object_at(&self, index: NSUInteger, buffer_desc: Option<&PipelineBufferDescriptorRef>) {
        unsafe { msg_send![self, setObject:buffer_desc atIndexedSubscript:index] }
    }
}

pub enum MTLPipelineBufferDescriptor {}

foreign_obj_type! {
    type CType = MTLPipelineBufferDescriptor;
    pub struct PipelineBufferDescriptor;
    pub struct PipelineBufferDescriptorRef;
}

impl PipelineBufferDescriptorRef {
    pub fn mutability(&self) -> MTLMutability {
        unsafe { msg_send![self, mutability] }
    }

    pub fn set_mutability(&self, new_mutability: MTLMutability) {
        unsafe { msg_send![self, setMutability: new_mutability] }
    }
}
