// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use block::Block;

#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum MTLCommandBufferStatus {
    NotEnqueued = 0,
    Enqueued = 1,
    Committed = 2,
    Scheduled = 3,
    Completed = 4,
    Error = 5,
}

#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum MTLCommandBufferError {
    None = 0,
    Internal = 1,
    Timeout = 2,
    PageFault = 3,
    Blacklisted = 4,
    NotPermitted = 7,
    OutOfMemory = 8,
    InvalidResource = 9,
    Memoryless = 10,
    DeviceRemoved = 11,
}

#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum MTLDispatchType {
    Serial = 0,
    Concurrent = 1,
}

type _MTLCommandBufferHandler = Block<(MTLCommandBuffer), ()>;

pub enum MTLCommandBuffer {}

foreign_obj_type! {
    type CType = MTLCommandBuffer;
    pub struct CommandBuffer;
    pub struct CommandBufferRef;
}

impl CommandBufferRef {
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

    pub fn enqueue(&self) {
        unsafe { msg_send![self, enqueue] }
    }

    pub fn commit(&self) {
        unsafe { msg_send![self, commit] }
    }

    pub fn status(&self) -> MTLCommandBufferStatus {
        unsafe { msg_send![self, status] }
    }

    pub fn present_drawable(&self, drawable: &DrawableRef) {
        unsafe { msg_send![self, presentDrawable: drawable] }
    }

    pub fn wait_until_completed(&self) {
        unsafe { msg_send![self, waitUntilCompleted] }
    }

    pub fn wait_until_scheduled(&self) {
        unsafe { msg_send![self, waitUntilScheduled] }
    }

    pub fn new_blit_command_encoder(&self) -> &BlitCommandEncoderRef {
        unsafe { msg_send![self, blitCommandEncoder] }
    }

    pub fn new_compute_command_encoder(&self) -> &ComputeCommandEncoderRef {
        unsafe { msg_send![self, computeCommandEncoder] }
    }

    pub fn new_render_command_encoder(
        &self,
        descriptor: &RenderPassDescriptorRef,
    ) -> &RenderCommandEncoderRef {
        unsafe { msg_send![self, renderCommandEncoderWithDescriptor: descriptor] }
    }

    pub fn new_parallel_render_command_encoder(
        &self,
        descriptor: &RenderPassDescriptorRef,
    ) -> &ParallelRenderCommandEncoderRef {
        unsafe { msg_send![self, parallelRenderCommandEncoderWithDescriptor: descriptor] }
    }

    pub fn compute_command_encoder_with_dispatch_type(
        &self,
        ty: MTLDispatchType,
    ) -> &ComputeCommandEncoderRef {
        unsafe { msg_send![self, computeCommandEncoderWithDispatchType: ty] }
    }
}
