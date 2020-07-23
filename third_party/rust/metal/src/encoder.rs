// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use cocoa::foundation::{NSInteger, NSRange, NSUInteger};

use std::ops::Range;

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLPrimitiveType {
    Point = 0,
    Line = 1,
    LineStrip = 2,
    Triangle = 3,
    TriangleStrip = 4,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLIndexType {
    UInt16 = 0,
    UInt32 = 1,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLVisibilityResultMode {
    Disabled = 0,
    Boolean = 1,
    Counting = 2,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLCullMode {
    None = 0,
    Front = 1,
    Back = 2,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLWinding {
    Clockwise = 0,
    CounterClockwise = 1,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLDepthClipMode {
    Clip = 0,
    Clamp = 1,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum MTLTriangleFillMode {
    Fill = 0,
    Lines = 1,
}

bitflags! {
    #[allow(non_upper_case_globals)]
    pub struct MTLBlitOption: NSUInteger {
        const DepthFromDepthStencil   = 1 << 0;
        const StencilFromDepthStencil = 1 << 1;
        const RowLinearPVRTC          = 1 << 2;
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct MTLScissorRect {
    pub x: NSUInteger,
    pub y: NSUInteger,
    pub width: NSUInteger,
    pub height: NSUInteger,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct MTLViewport {
    pub originX: f64,
    pub originY: f64,
    pub width: f64,
    pub height: f64,
    pub znear: f64,
    pub zfar: f64,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct MTLDrawPrimitivesIndirectArguments {
    pub vertexCount: u32,
    pub instanceCount: u32,
    pub vertexStart: u32,
    pub baseInstance: u32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct MTLDrawIndexedPrimitivesIndirectArguments {
    pub indexCount: u32,
    pub instanceCount: u32,
    pub indexStart: u32,
    pub baseVertex: i32,
    pub baseInstance: u32,
}

pub enum MTLCommandEncoder {}

foreign_obj_type! {
    type CType = MTLCommandEncoder;
    pub struct CommandEncoder;
    pub struct CommandEncoderRef;
}

impl CommandEncoderRef {
    pub fn label(&self) -> &str {
        unsafe {
            let label = msg_send![self, label];
            crate::nsstring_as_str(label)
        }
    }

    pub fn set_label(&self, label: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(label);
            msg_send![self, setLabel: nslabel]
        }
    }

    pub fn end_encoding(&self) {
        unsafe {
            msg_send![self, endEncoding]
        }
    }

    pub fn insert_debug_signpost(&self, name: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(name);
            msg_send![self, insertDebugSignpost: nslabel]
        }
    }

    pub fn push_debug_group(&self, name: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(name);
            msg_send![self, pushDebugGroup: nslabel]
        }
    }

    pub fn pop_debug_group(&self) {
        unsafe {
            msg_send![self, popDebugGroup]
        }
    }
}

pub enum MTLParallelRenderCommandEncoder {}

foreign_obj_type! {
    type CType = MTLParallelRenderCommandEncoder;
    pub struct ParallelRenderCommandEncoder;
    pub struct ParallelRenderCommandEncoderRef;
    type ParentType = CommandEncoderRef;
}

impl ParallelRenderCommandEncoderRef {
    pub fn render_command_encoder(&self) -> &RenderCommandEncoderRef {
        unsafe { msg_send![self, renderCommandEncoder] }
    }
}

pub enum MTLRenderCommandEncoder {}

foreign_obj_type! {
    type CType = MTLRenderCommandEncoder;
    pub struct RenderCommandEncoder;
    pub struct RenderCommandEncoderRef;
    type ParentType = CommandEncoderRef;
}

impl RenderCommandEncoderRef {
    pub fn set_render_pipeline_state(&self, pipeline_state: &RenderPipelineStateRef) {
        unsafe { msg_send![self, setRenderPipelineState: pipeline_state] }
    }

    pub fn set_viewport(&self, viewport: MTLViewport) {
        unsafe { msg_send![self, setViewport: viewport] }
    }

    pub fn set_front_facing_winding(&self, winding: MTLWinding) {
        unsafe { msg_send![self, setFrontFacingWinding: winding] }
    }

    pub fn set_cull_mode(&self, mode: MTLCullMode) {
        unsafe { msg_send![self, setCullMode: mode] }
    }

    pub fn set_depth_clip_mode(&self, mode: MTLDepthClipMode) {
        unsafe { msg_send![self, setDepthClipMode: mode] }
    }

    pub fn set_depth_bias(&self, bias: f32, scale: f32, clamp: f32) {
        unsafe {
            msg_send![self, setDepthBias:bias
                              slopeScale:scale
                                   clamp:clamp]
        }
    }

    pub fn set_scissor_rect(&self, rect: MTLScissorRect) {
        unsafe { msg_send![self, setScissorRect: rect] }
    }

    pub fn set_triangle_fill_mode(&self, mode: MTLTriangleFillMode) {
        unsafe { msg_send![self, setTriangleFillMode: mode] }
    }

    pub fn set_blend_color(&self, red: f32, green: f32, blue: f32, alpha: f32) {
        unsafe {
            msg_send![self, setBlendColorRed:red
                                         green:green
                                          blue:blue
                                         alpha:alpha]
        }
    }

    pub fn set_depth_stencil_state(&self, depth_stencil_state: &DepthStencilStateRef) {
        unsafe { msg_send![self, setDepthStencilState: depth_stencil_state] }
    }

    pub fn set_stencil_reference_value(&self, value: u32) {
        unsafe { msg_send![self, setStencilReferenceValue: value] }
    }

    pub fn set_stencil_front_back_reference_value(&self, front: u32, back: u32) {
        unsafe {
            msg_send![self, setStencilFrontReferenceValue:front
                                       backReferenceValue:back]
        }
    }

    pub fn set_visibility_result_mode(&self, mode: MTLVisibilityResultMode, offset: NSUInteger) {
        unsafe {
            msg_send![self, setVisibilityResultMode:mode
                                             offset:offset]
        }
    }

    // Specifying Resources for a Vertex Shader Function

    pub fn set_vertex_bytes(
        &self,
        index: NSUInteger,
        length: NSUInteger,
        bytes: *const std::ffi::c_void,
    ) {
        unsafe {
            msg_send![self,
                setVertexBytes:bytes
                length:length
                atIndex:index
            ]
        }
    }

    pub fn set_vertex_buffer(
        &self,
        index: NSUInteger,
        buffer: Option<&BufferRef>,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setVertexBuffer:buffer
                offset:offset
                atIndex:index
            ]
        }
    }

    pub fn set_vertex_buffer_offset(
        &self,
        index: NSUInteger,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setVertexBufferOffset:offset
                atIndex:index
            ]
        }
    }

    pub fn set_vertex_buffers(
        &self,
        start_index: NSUInteger,
        data: &[Option<&BufferRef>],
        offsets: &[NSUInteger],
    ) {
        debug_assert_eq!(offsets.len(), data.len());
        unsafe {
            msg_send![self,
                setVertexBuffers: data.as_ptr()
                offsets: offsets.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_vertex_texture(&self, index: NSUInteger, texture: Option<&TextureRef>) {
        unsafe {
            msg_send![self,
                setVertexTexture:texture
                atIndex:index
            ]
        }
    }

    pub fn set_vertex_textures(&self, start_index: NSUInteger, data: &[Option<&TextureRef>]) {
        unsafe {
            msg_send![self,
                setVertexTextures: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_vertex_sampler_state(&self, index: NSUInteger, sampler: Option<&SamplerStateRef>) {
        unsafe {
            msg_send![self,
                setVertexSamplerState:sampler
                atIndex:index
            ]
        }
    }

    pub fn set_vertex_sampler_states(
        &self,
        start_index: NSUInteger,
        data: &[Option<&SamplerStateRef>],
    ) {
        unsafe {
            msg_send![self,
                setVertexSamplerStates: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_vertex_sampler_state_with_lod(
        &self,
        index: NSUInteger,
        sampler: Option<&SamplerStateRef>,
        lod_clamp: Range<f32>,
    ) {
        unsafe {
            msg_send![self,
                setVertexSamplerState:sampler
                lodMinClamp:lod_clamp.start
                lodMaxClamp:lod_clamp.end
                atIndex:index
            ]
        }
    }

    // Specifying Resources for a Fragment Shader Function

    pub fn set_fragment_bytes(
        &self,
        index: NSUInteger,
        length: NSUInteger,
        bytes: *const std::ffi::c_void,
    ) {
        unsafe {
            msg_send![self,
                setFragmentBytes:bytes
                length:length
                atIndex:index
            ]
        }
    }

    pub fn set_fragment_buffer(
        &self,
        index: NSUInteger,
        buffer: Option<&BufferRef>,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setFragmentBuffer:buffer
                offset:offset
                atIndex:index
            ]
        }
    }

    pub fn set_fragment_buffer_offset(
        &self,
        index: NSUInteger,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setFragmentBufferOffset:offset
                atIndex:index
            ]
        }
    }

    pub fn set_fragment_buffers(
        &self,
        start_index: NSUInteger,
        data: &[Option<&BufferRef>],
        offsets: &[NSUInteger],
    ) {
        debug_assert_eq!(offsets.len(), data.len());
        unsafe {
            msg_send![self,
                setFragmentBuffers: data.as_ptr()
                offsets: offsets.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_fragment_texture(&self, index: NSUInteger, texture: Option<&TextureRef>) {
        unsafe {
            msg_send![self,
                setFragmentTexture:texture
                atIndex:index
            ]
        }
    }

    pub fn set_fragment_textures(&self, start_index: NSUInteger, data: &[Option<&TextureRef>]) {
        unsafe {
            msg_send![self,
                setFragmentTextures: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_fragment_sampler_state(&self, index: NSUInteger, sampler: Option<&SamplerStateRef>) {
        unsafe {
            msg_send![self, setFragmentSamplerState:sampler
                                            atIndex:index]
        }
    }

    pub fn set_fragment_sampler_states(
        &self,
        start_index: NSUInteger,
        data: &[Option<&SamplerStateRef>],
    ) {
        unsafe {
            msg_send![self,
                setFragmentSamplerStates: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_fragment_sampler_state_with_lod(
        &self,
        index: NSUInteger,
        sampler: Option<&SamplerStateRef>,
        lod_clamp: Range<f32>,
    ) {
        unsafe {
            msg_send![self,
                setFragmentSamplerState:sampler
                lodMinClamp:lod_clamp.start
                lodMaxClamp:lod_clamp.end
                atIndex:index
            ]
        }
    }

    // Drawing Geometric Primitives

    pub fn draw_primitives(
        &self,
        primitive_type: MTLPrimitiveType,
        vertex_start: NSUInteger,
        vertex_count: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPrimitives: primitive_type
                vertexStart: vertex_start
                vertexCount: vertex_count
            ]
        }
    }

    pub fn draw_primitives_instanced(
        &self,
        primitive_type: MTLPrimitiveType,
        vertex_start: NSUInteger,
        vertex_count: NSUInteger,
        instance_count: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPrimitives: primitive_type
                vertexStart: vertex_start
                vertexCount: vertex_count
                instanceCount: instance_count
            ]
        }
    }

    pub fn draw_primitives_instanced_base_instance(
        &self,
        primitive_type: MTLPrimitiveType,
        vertex_start: NSUInteger,
        vertex_count: NSUInteger,
        instance_count: NSUInteger,
        base_instance: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPrimitives: primitive_type
                vertexStart: vertex_start
                vertexCount: vertex_count
                instanceCount: instance_count
                baseInstance: base_instance
            ]
        }
    }

    pub fn draw_primitives_indirect(
        &self,
        primitive_type: MTLPrimitiveType,
        indirect_buffer: &BufferRef,
        indirect_buffer_offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawPrimitives: primitive_type
                indirectBuffer: indirect_buffer
                indirectBufferOffset: indirect_buffer_offset
            ]
        }
    }

    pub fn draw_indexed_primitives(
        &self,
        primitive_type: MTLPrimitiveType,
        index_count: NSUInteger,
        index_type: MTLIndexType,
        index_buffer: &BufferRef,
        index_buffer_offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPrimitives: primitive_type
                indexCount: index_count
                indexType: index_type
                indexBuffer: index_buffer
                indexBufferOffset: index_buffer_offset
            ]
        }
    }

    pub fn draw_indexed_primitives_instanced(
        &self,
        primitive_type: MTLPrimitiveType,
        index_count: NSUInteger,
        index_type: MTLIndexType,
        index_buffer: &BufferRef,
        index_buffer_offset: NSUInteger,
        instance_count: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPrimitives: primitive_type
                indexCount: index_count
                indexType: index_type
                indexBuffer: index_buffer
                indexBufferOffset: index_buffer_offset
                instanceCount: instance_count
            ]
        }
    }

    pub fn draw_indexed_primitives_instanced_base_instance(
        &self,
        primitive_type: MTLPrimitiveType,
        index_count: NSUInteger,
        index_type: MTLIndexType,
        index_buffer: &BufferRef,
        index_buffer_offset: NSUInteger,
        instance_count: NSUInteger,
        base_vertex: NSInteger,
        base_instance: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPrimitives: primitive_type
                indexCount: index_count
                indexType: index_type
                indexBuffer: index_buffer
                indexBufferOffset: index_buffer_offset
                instanceCount: instance_count
                baseVertex: base_vertex
                baseInstance: base_instance
            ]
        }
    }

    pub fn draw_indexed_primitives_indirect(
        &self,
        primitive_type: MTLPrimitiveType,
        index_type: MTLIndexType,
        index_buffer: &BufferRef,
        index_buffer_offset: NSUInteger,
        indirect_buffer: &BufferRef,
        indirect_buffer_offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                drawIndexedPrimitives: primitive_type
                indexType: index_type
                indexBuffer: index_buffer
                indexBufferOffset: index_buffer_offset
                indirectBuffer: indirect_buffer
                indirectBufferOffset: indirect_buffer_offset
            ]
        }
    }

    // TODO: more draws
    // fn setVertexBufferOffset_atIndex(self, offset: NSUInteger, index: NSUInteger);
    // fn setVertexBuffers_offsets_withRange(self, buffers: *const id, offsets: *const NSUInteger, range: NSRange);
    // fn setVertexSamplerStates_lodMinClamps_lodMaxClamps_withRange(self, samplers: *const id, lodMinClamps: *const f32, lodMaxClamps: *const f32, range: NSRange);

    pub fn use_resource(&self, resource: &ResourceRef, usage: MTLResourceUsage) {
        unsafe {
            msg_send![self, useResource:resource
                                  usage:usage]
        }
    }

    pub fn use_heap(&self, heap: &HeapRef) {
        unsafe { msg_send![self, useHeap: heap] }
    }
}

pub enum MTLBlitCommandEncoder {}

foreign_obj_type! {
    type CType = MTLBlitCommandEncoder;
    pub struct BlitCommandEncoder;
    pub struct BlitCommandEncoderRef;
    type ParentType = CommandEncoderRef;
}

impl BlitCommandEncoderRef {
    pub fn synchronize_resource(&self, resource: &ResourceRef) {
        unsafe { msg_send![self, synchronizeResource: resource] }
    }

    pub fn fill_buffer(&self, destination_buffer: &BufferRef, range: NSRange, value: u8) {
        unsafe {
            msg_send![self,
                fillBuffer: destination_buffer
                range: range
                value: value
            ]
        }
    }

    pub fn copy_from_buffer(
        &self,
        source_buffer: &BufferRef,
        source_offset: NSUInteger,
        destination_buffer: &BufferRef,
        destination_offset: NSUInteger,
        size: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                copyFromBuffer: source_buffer
                sourceOffset: source_offset
                toBuffer: destination_buffer
                destinationOffset: destination_offset
                size: size
            ]
        }
    }

    pub fn copy_from_texture(
        &self,
        source_texture: &TextureRef,
        source_slice: NSUInteger,
        source_level: NSUInteger,
        source_origin: MTLOrigin,
        source_size: MTLSize,
        destination_texture: &TextureRef,
        destination_slice: NSUInteger,
        destination_level: NSUInteger,
        destination_origin: MTLOrigin,
    ) {
        unsafe {
            msg_send![self,
                copyFromTexture: source_texture
                sourceSlice: source_slice
                sourceLevel: source_level
                sourceOrigin: source_origin
                sourceSize: source_size
                toTexture: destination_texture
                destinationSlice: destination_slice
                destinationLevel: destination_level
                destinationOrigin: destination_origin
            ]
        }
    }

    pub fn copy_from_buffer_to_texture(
        &self,
        source_buffer: &BufferRef,
        source_offset: NSUInteger,
        source_bytes_per_row: NSUInteger,
        source_bytes_per_image: NSUInteger,
        source_size: MTLSize,
        destination_texture: &TextureRef,
        destination_slice: NSUInteger,
        destination_level: NSUInteger,
        destination_origin: MTLOrigin,
        options: MTLBlitOption,
    ) {
        unsafe {
            msg_send![self,
                copyFromBuffer: source_buffer
                sourceOffset: source_offset
                sourceBytesPerRow: source_bytes_per_row
                sourceBytesPerImage: source_bytes_per_image
                sourceSize: source_size
                toTexture: destination_texture
                destinationSlice: destination_slice
                destinationLevel: destination_level
                destinationOrigin: destination_origin
                options: options
            ]
        }
    }

    pub fn copy_from_texture_to_buffer(
        &self,
        source_texture: &TextureRef,
        source_slice: NSUInteger,
        source_level: NSUInteger,
        source_origin: MTLOrigin,
        source_size: MTLSize,
        destination_buffer: &BufferRef,
        destination_offset: NSUInteger,
        destination_bytes_per_row: NSUInteger,
        destination_bytes_per_image: NSUInteger,
        options: MTLBlitOption,
    ) {
        unsafe {
            msg_send![self,
                copyFromTexture: source_texture
                sourceSlice: source_slice
                sourceLevel: source_level
                sourceOrigin: source_origin
                sourceSize: source_size
                toBuffer: destination_buffer
                destinationOffset: destination_offset
                destinationBytesPerRow: destination_bytes_per_row
                destinationBytesPerImage: destination_bytes_per_image
                options: options
            ]
        }
    }

    pub fn optimize_contents_for_gpu_access(&self, texture: &TextureRef) {
        unsafe { msg_send![self, optimizeContentsForGPUAccess: texture] }
    }

    pub fn optimize_contents_for_gpu_access_slice_level(
        &self,
        texture: &TextureRef,
        slice: NSUInteger,
        level: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                optimizeContentsForGPUAccess: texture
                slice: slice
                level: level
            ]
        }
    }

    pub fn optimize_contents_for_cpu_access(&self, texture: &TextureRef) {
        unsafe { msg_send![self, optimizeContentsForCPUAccess: texture] }
    }

    pub fn optimize_contents_for_cpu_access_slice_level(
        &self,
        texture: &TextureRef,
        slice: NSUInteger,
        level: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                optimizeContentsForCPUAccess: texture
                slice: slice
                level: level
            ]
        }
    }
}

pub enum MTLComputeCommandEncoder {}

foreign_obj_type! {
    type CType = MTLComputeCommandEncoder;
    pub struct ComputeCommandEncoder;
    pub struct ComputeCommandEncoderRef;
    type ParentType = CommandEncoderRef;
}

impl ComputeCommandEncoderRef {
    pub fn set_compute_pipeline_state(&self, state: &ComputePipelineStateRef) {
        unsafe { msg_send![self, setComputePipelineState: state] }
    }

    pub fn set_buffer(&self, index: NSUInteger, buffer: Option<&BufferRef>, offset: NSUInteger) {
        unsafe { msg_send![self, setBuffer:buffer offset:offset atIndex:index] }
    }

    pub fn set_buffers(
        &self,
        start_index: NSUInteger,
        data: &[Option<&BufferRef>],
        offsets: &[NSUInteger],
    ) {
        debug_assert_eq!(offsets.len(), data.len());
        unsafe {
            msg_send![self,
                setBuffers: data.as_ptr()
                offsets: offsets.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_texture(&self, index: NSUInteger, texture: Option<&TextureRef>) {
        unsafe {
            msg_send![self,
                setTexture:texture
                atIndex:index
            ]
        }
    }

    pub fn set_textures(&self, start_index: NSUInteger, data: &[Option<&TextureRef>]) {
        unsafe {
            msg_send![self,
                setTextures: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_sampler_state(&self, index: NSUInteger, sampler: Option<&SamplerStateRef>) {
        unsafe {
            msg_send![self,
                setSamplerState:sampler
                atIndex:index
            ]
        }
    }

    pub fn set_sampler_states(&self, start_index: NSUInteger, data: &[Option<&SamplerStateRef>]) {
        unsafe {
            msg_send![self,
                setSamplerStates: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_sampler_state_with_lod(
        &self,
        index: NSUInteger,
        sampler: Option<&SamplerStateRef>,
        lod_clamp: Range<f32>,
    ) {
        unsafe {
            msg_send![self,
                setSamplerState:sampler
                lodMinClamp:lod_clamp.start
                lodMaxClamp:lod_clamp.end
                atIndex:index
            ]
        }
    }

    pub fn set_bytes(&self, index: NSUInteger, length: NSUInteger, bytes: *const std::ffi::c_void) {
        unsafe {
            msg_send![self,
                setBytes: bytes
                length: length
                atIndex: index
            ]
        }
    }

    pub fn dispatch_thread_groups(
        &self,
        thread_groups_count: MTLSize,
        threads_per_thread_group: MTLSize,
    ) {
        unsafe {
            msg_send![self,
                dispatchThreadgroups:thread_groups_count
                threadsPerThreadgroup:threads_per_thread_group
            ]
        }
    }

    pub fn dispatch_thread_groups_indirect(
        &self,
        buffer: &BufferRef,
        offset: NSUInteger,
        threads_per_thread_group: MTLSize,
    ) {
        unsafe {
            msg_send![self,
                dispatchThreadgroupsWithIndirectBuffer:buffer
                indirectBufferOffset:offset
                threadsPerThreadgroup:threads_per_thread_group
            ]
        }
    }

    pub fn use_resource(&self, resource: &ResourceRef, usage: MTLResourceUsage) {
        unsafe {
            msg_send![self,
                useResource:resource
                usage:usage
            ]
        }
    }

    pub fn use_heap(&self, heap: &HeapRef) {
        unsafe { msg_send![self, useHeap: heap] }
    }
}

pub enum MTLArgumentEncoder {}

foreign_obj_type! {
    type CType = MTLArgumentEncoder;
    pub struct ArgumentEncoder;
    pub struct ArgumentEncoderRef;
}

impl ArgumentEncoderRef {
    pub fn encoded_length(&self) -> NSUInteger {
        unsafe { msg_send![self, encodedLength] }
    }

    pub fn alignment(&self) -> NSUInteger {
        unsafe { msg_send![self, alignment] }
    }

    pub fn set_argument_buffer(&self, buffer: &BufferRef, offset: NSUInteger) {
        unsafe {
            msg_send![self,
                setArgumentBuffer: buffer
                offset: offset
            ]
        }
    }

    pub fn set_argument_buffer_to_element(
        &self,
        array_element: NSUInteger,
        buffer: &BufferRef,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setArgumentBuffer: buffer
                startOffset: offset
                arrayElement: array_element
            ]
        }
    }

    pub fn set_buffer(
        &self,
        at_index: NSUInteger,
        buffer: &BufferRef,
        offset: NSUInteger,
    ) {
        unsafe {
            msg_send![self,
                setBuffer: buffer
                offset: offset
                atIndex: at_index
            ]
        }
    }

    pub fn set_buffers(
        &self,
        start_index: NSUInteger,
        data: &[&BufferRef],
        offsets: &[NSUInteger],
    ) {
        assert_eq!(offsets.len(), data.len());
        unsafe {
            msg_send![self,
                setBuffers: data.as_ptr()
                offsets: offsets.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_texture(&self, at_index: NSUInteger, texture: &TextureRef) {
        unsafe {
            msg_send![self,
                setTexture: texture
                atIndex: at_index
            ]
        }
    }

    pub fn set_textures(&self, start_index: NSUInteger, data: &[&TextureRef]) {
        unsafe {
            msg_send![self,
                setTextures: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn set_sampler_state(&self, at_index: NSUInteger, sampler_state: &SamplerStateRef) {
        unsafe {
            msg_send![self,
                setSamplerState: sampler_state
                atIndex: at_index
            ]
        }
    }

    pub fn set_sampler_states(&self, start_index: NSUInteger, data: &[&SamplerStateRef]) {
        unsafe {
            msg_send![self,
                setSamplerStates: data.as_ptr()
                withRange: NSRange {
                    location: start_index,
                    length: data.len() as _,
                }
            ]
        }
    }

    pub fn constant_data(&self, at_index: NSUInteger) -> *mut std::ffi::c_void {
        unsafe { msg_send![self, constantDataAtIndex: at_index] }
    }

    pub fn new_argument_encoder_for_buffer(&self, index: NSUInteger) -> ArgumentEncoder {
        unsafe {
            let ptr = msg_send![self, newArgumentEncoderForBufferAtIndex: index];
            ArgumentEncoder::from_ptr(ptr)
        }
    }
}
