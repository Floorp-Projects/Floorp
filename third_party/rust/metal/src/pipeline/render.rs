// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use cocoa::foundation::NSUInteger;
use objc::runtime::{NO, YES};

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLBlendFactor {
    Zero = 0,
    One = 1,
    SourceColor = 2,
    OneMinusSourceColor = 3,
    SourceAlpha = 4,
    OneMinusSourceAlpha = 5,
    DestinationColor = 6,
    OneMinusDestinationColor = 7,
    DestinationAlpha = 8,
    OneMinusDestinationAlpha = 9,
    SourceAlphaSaturated = 10,
    BlendColor = 11,
    OneMinusBlendColor = 12,
    BlendAlpha = 13,
    OneMinusBlendAlpha = 14,
    Source1Color = 15,
    OneMinusSource1Color = 16,
    Source1Alpha = 17,
    OneMinusSource1Alpha = 18,
}

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLBlendOperation {
    Add = 0,
    Subtract = 1,
    ReverseSubtract = 2,
    Min = 3,
    Max = 4,
}

bitflags! {
    pub struct MTLColorWriteMask: NSUInteger {
        const Red   = 0x1 << 3;
        const Green = 0x1 << 2;
        const Blue  = 0x1 << 1;
        const Alpha = 0x1 << 0;
    }
}

#[repr(u64)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum MTLPrimitiveTopologyClass {
    Unspecified = 0,
    Point = 1,
    Line = 2,
    Triangle = 3,
}

pub enum MTLRenderPipelineColorAttachmentDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPipelineColorAttachmentDescriptor;
    pub struct RenderPipelineColorAttachmentDescriptor;
    pub struct RenderPipelineColorAttachmentDescriptorRef;
}

impl RenderPipelineColorAttachmentDescriptorRef {
    pub fn pixel_format(&self) -> MTLPixelFormat {
        unsafe { msg_send![self, pixelFormat] }
    }

    pub fn set_pixel_format(&self, pixel_format: MTLPixelFormat) {
        unsafe { msg_send![self, setPixelFormat: pixel_format] }
    }

    pub fn is_blending_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, isBlendingEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_blending_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setBlendingEnabled: enabled] }
    }

    pub fn source_rgb_blend_factor(&self) -> MTLBlendFactor {
        unsafe { msg_send![self, sourceRGBBlendFactor] }
    }

    pub fn set_source_rgb_blend_factor(&self, blend_factor: MTLBlendFactor) {
        unsafe { msg_send![self, setSourceRGBBlendFactor: blend_factor] }
    }

    pub fn destination_rgb_blend_factor(&self) -> MTLBlendFactor {
        unsafe { msg_send![self, destinationRGBBlendFactor] }
    }

    pub fn set_destination_rgb_blend_factor(&self, blend_factor: MTLBlendFactor) {
        unsafe { msg_send![self, setDestinationRGBBlendFactor: blend_factor] }
    }

    pub fn rgb_blend_operation(&self) -> MTLBlendOperation {
        unsafe { msg_send![self, rgbBlendOperation] }
    }

    pub fn set_rgb_blend_operation(&self, blend_operation: MTLBlendOperation) {
        unsafe { msg_send![self, setRgbBlendOperation: blend_operation] }
    }

    pub fn source_alpha_blend_factor(&self) -> MTLBlendFactor {
        unsafe { msg_send![self, sourceAlphaBlendFactor] }
    }

    pub fn set_source_alpha_blend_factor(&self, blend_factor: MTLBlendFactor) {
        unsafe { msg_send![self, setSourceAlphaBlendFactor: blend_factor] }
    }

    pub fn destination_alpha_blend_factor(&self) -> MTLBlendFactor {
        unsafe { msg_send![self, destinationAlphaBlendFactor] }
    }

    pub fn set_destination_alpha_blend_factor(&self, blend_factor: MTLBlendFactor) {
        unsafe { msg_send![self, setDestinationAlphaBlendFactor: blend_factor] }
    }

    pub fn alpha_blend_operation(&self) -> MTLBlendOperation {
        unsafe { msg_send![self, alphaBlendOperation] }
    }

    pub fn set_alpha_blend_operation(&self, blend_operation: MTLBlendOperation) {
        unsafe { msg_send![self, setAlphaBlendOperation: blend_operation] }
    }

    pub fn write_mask(&self) -> MTLColorWriteMask {
        unsafe { msg_send![self, writeMask] }
    }

    pub fn set_write_mask(&self, mask: MTLColorWriteMask) {
        unsafe { msg_send![self, setWriteMask: mask] }
    }
}

pub enum MTLRenderPipelineReflection {}

foreign_obj_type! {
    type CType = MTLRenderPipelineReflection;
    pub struct RenderPipelineReflection;
    pub struct RenderPipelineReflectionRef;
}

impl RenderPipelineReflection {
    #[cfg(feature = "private")]
    pub unsafe fn new(
        vertex_data: *mut std::ffi::c_void,
        fragment_data: *mut std::ffi::c_void,
        vertex_desc: *mut std::ffi::c_void,
        device: &DeviceRef,
        options: u64,
        flags: u64,
    ) -> Self {
        let class = class!(MTLRenderPipelineReflection);
        let this: RenderPipelineReflection = msg_send![class, alloc];
        let this_alias: *mut Object = msg_send![this.as_ref(), initWithVertexData:vertex_data
                                                                fragmentData:fragment_data
                                                serializedVertexDescriptor:vertex_desc
                                                                    device:device
                                                                    options:options
                                                                        flags:flags];
        if this_alias.is_null() {
            panic!("[MTLRenderPipelineReflection init] failed");
        }
        this
    }
}

impl RenderPipelineReflectionRef {
    pub fn fragment_arguments(&self) -> &Array<Argument> {
        unsafe { msg_send![self, fragmentArguments] }
    }

    pub fn vertex_arguments(&self) -> &Array<Argument> {
        unsafe { msg_send![self, vertexArguments] }
    }
}

pub enum MTLRenderPipelineDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPipelineDescriptor;
    pub struct RenderPipelineDescriptor;
    pub struct RenderPipelineDescriptorRef;
}

impl RenderPipelineDescriptor {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLRenderPipelineDescriptor);
            msg_send![class, new]
        }
    }
}

impl RenderPipelineDescriptorRef {
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

    pub fn vertex_function(&self) -> Option<&FunctionRef> {
        unsafe { msg_send![self, vertexFunction] }
    }

    pub fn set_vertex_function(&self, function: Option<&FunctionRef>) {
        unsafe { msg_send![self, setVertexFunction: function] }
    }

    pub fn fragment_function(&self) -> Option<&FunctionRef> {
        unsafe { msg_send![self, fragmentFunction] }
    }

    pub fn set_fragment_function(&self, function: Option<&FunctionRef>) {
        unsafe { msg_send![self, setFragmentFunction: function] }
    }

    pub fn vertex_descriptor(&self) -> Option<&VertexDescriptorRef> {
        unsafe { msg_send![self, vertexDescriptor] }
    }

    pub fn set_vertex_descriptor(&self, descriptor: Option<&VertexDescriptorRef>) {
        unsafe { msg_send![self, setVertexDescriptor: descriptor] }
    }

    pub fn sample_count(&self) -> NSUInteger {
        unsafe { msg_send![self, sampleCount] }
    }

    pub fn set_sample_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setSampleCount: count] }
    }

    pub fn is_alpha_to_coverage_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, isAlphaToCoverageEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_alpha_to_coverage_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setAlphaToCoverageEnabled: enabled] }
    }

    pub fn is_alpha_to_one_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, isAlphaToOneEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_alpha_to_one_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setAlphaToOneEnabled: enabled] }
    }

    pub fn is_rasterization_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, isRasterizationEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_rasterization_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setRasterizationEnabled: enabled] }
    }

    pub fn color_attachments(&self) -> &RenderPipelineColorAttachmentDescriptorArrayRef {
        unsafe { msg_send![self, colorAttachments] }
    }

    pub fn depth_attachment_pixel_format(&self) -> MTLPixelFormat {
        unsafe { msg_send![self, depthAttachmentPixelFormat] }
    }

    pub fn set_depth_attachment_pixel_format(&self, pixel_format: MTLPixelFormat) {
        unsafe { msg_send![self, setDepthAttachmentPixelFormat: pixel_format] }
    }

    pub fn stencil_attachment_pixel_format(&self) -> MTLPixelFormat {
        unsafe { msg_send![self, stencilAttachmentPixelFormat] }
    }

    pub fn set_stencil_attachment_pixel_format(&self, pixel_format: MTLPixelFormat) {
        unsafe { msg_send![self, setStencilAttachmentPixelFormat: pixel_format] }
    }

    pub fn input_primitive_topology(&self) -> MTLPrimitiveTopologyClass {
        unsafe { msg_send![self, inputPrimitiveTopology] }
    }

    pub fn set_input_primitive_topology(&self, topology: MTLPrimitiveTopologyClass) {
        unsafe { msg_send![self, setInputPrimitiveTopology: topology] }
    }

    #[cfg(feature = "private")]
    pub unsafe fn serialize_vertex_data(&self) -> *mut std::ffi::c_void {
        use std::ptr;
        let flags = 0;
        let err: *mut Object = ptr::null_mut();
        msg_send![self, newSerializedVertexDataWithFlags:flags
                                                    error:err]
    }

    #[cfg(feature = "private")]
    pub unsafe fn serialize_fragment_data(&self) -> *mut std::ffi::c_void {
        msg_send![self, serializeFragmentData]
    }

    pub fn support_indirect_command_buffers(&self) -> bool {
        unsafe {
            match msg_send![self, supportIndirectCommandBuffers] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_support_indirect_command_buffers(&self, support: bool) {
        unsafe { msg_send![self, setSupportIndirectCommandBuffers: support] }
    }
}

pub enum MTLRenderPipelineState {}

foreign_obj_type! {
    type CType = MTLRenderPipelineState;
    pub struct RenderPipelineState;
    pub struct RenderPipelineStateRef;
}

impl RenderPipelineStateRef {
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
}

pub enum MTLRenderPipelineColorAttachmentDescriptorArray {}

foreign_obj_type! {
    type CType = MTLRenderPipelineColorAttachmentDescriptorArray;
    pub struct RenderPipelineColorAttachmentDescriptorArray;
    pub struct RenderPipelineColorAttachmentDescriptorArrayRef;
}

impl RenderPipelineColorAttachmentDescriptorArrayRef {
    pub fn object_at(&self, index: usize) -> Option<&RenderPipelineColorAttachmentDescriptorRef> {
        unsafe { msg_send![self, objectAtIndexedSubscript: index] }
    }

    pub fn set_object_at(
        &self,
        index: usize,
        attachment: Option<&RenderPipelineColorAttachmentDescriptorRef>,
    ) {
        unsafe {
            msg_send![self, setObject:attachment
                   atIndexedSubscript:index]
        }
    }
}
