// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use super::*;

use cocoa_foundation::foundation::NSUInteger;

#[repr(u64)]
#[derive(Copy, Clone, Debug)]
pub enum MTLLoadAction {
    DontCare = 0,
    Load = 1,
    Clear = 2,
}

#[repr(u64)]
#[derive(Copy, Clone, Debug)]
pub enum MTLStoreAction {
    DontCare = 0,
    Store = 1,
    MultisampleResolve = 2,
    StoreAndMultisampleResolve = 3,
    Unknown = 4,
    CustomSampleDepthStore = 5,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct MTLClearColor {
    pub red: f64,
    pub green: f64,
    pub blue: f64,
    pub alpha: f64,
}

impl MTLClearColor {
    #[inline]
    pub fn new(red: f64, green: f64, blue: f64, alpha: f64) -> Self {
        Self {
            red,
            green,
            blue,
            alpha,
        }
    }
}

#[repr(u32)]
#[allow(non_camel_case_types)]
pub enum MTLMultisampleStencilResolveFilter {
    Sample0 = 0,
    DepthResolvedSample = 1,
}

pub enum MTLRenderPassAttachmentDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPassAttachmentDescriptor;
    pub struct RenderPassAttachmentDescriptor;
    pub struct RenderPassAttachmentDescriptorRef;
}

impl RenderPassAttachmentDescriptorRef {
    pub fn texture(&self) -> Option<&TextureRef> {
        unsafe { msg_send![self, texture] }
    }

    pub fn set_texture(&self, texture: Option<&TextureRef>) {
        unsafe { msg_send![self, setTexture: texture] }
    }

    pub fn level(&self) -> NSUInteger {
        unsafe { msg_send![self, level] }
    }

    pub fn set_level(&self, level: NSUInteger) {
        unsafe { msg_send![self, setLevel: level] }
    }

    pub fn slice(&self) -> NSUInteger {
        unsafe { msg_send![self, slice] }
    }

    pub fn set_slice(&self, slice: NSUInteger) {
        unsafe { msg_send![self, setSlice: slice] }
    }

    pub fn depth_plane(&self) -> NSUInteger {
        unsafe { msg_send![self, depthPlane] }
    }

    pub fn set_depth_plane(&self, depth_plane: NSUInteger) {
        unsafe { msg_send![self, setDepthPlane: depth_plane] }
    }

    pub fn resolve_texture(&self) -> Option<&TextureRef> {
        unsafe { msg_send![self, resolveTexture] }
    }

    pub fn set_resolve_texture(&self, resolve_texture: Option<&TextureRef>) {
        unsafe { msg_send![self, setResolveTexture: resolve_texture] }
    }

    pub fn resolve_level(&self) -> NSUInteger {
        unsafe { msg_send![self, resolveLevel] }
    }

    pub fn set_resolve_level(&self, resolve_level: NSUInteger) {
        unsafe { msg_send![self, setResolveLevel: resolve_level] }
    }

    pub fn resolve_slice(&self) -> NSUInteger {
        unsafe { msg_send![self, resolveSlice] }
    }

    pub fn set_resolve_slice(&self, resolve_slice: NSUInteger) {
        unsafe { msg_send![self, setResolveSlice: resolve_slice] }
    }

    pub fn resolve_depth_plane(&self) -> NSUInteger {
        unsafe { msg_send![self, resolveDepthPlane] }
    }

    pub fn set_resolve_depth_plane(&self, resolve_depth_plane: NSUInteger) {
        unsafe { msg_send![self, setResolveDepthPlane: resolve_depth_plane] }
    }

    pub fn load_action(&self) -> MTLLoadAction {
        unsafe { msg_send![self, loadAction] }
    }

    pub fn set_load_action(&self, load_action: MTLLoadAction) {
        unsafe { msg_send![self, setLoadAction: load_action] }
    }

    pub fn store_action(&self) -> MTLStoreAction {
        unsafe { msg_send![self, storeAction] }
    }

    pub fn set_store_action(&self, store_action: MTLStoreAction) {
        unsafe { msg_send![self, setStoreAction: store_action] }
    }
}

pub enum MTLRenderPassColorAttachmentDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPassColorAttachmentDescriptor;
    pub struct RenderPassColorAttachmentDescriptor;
    pub struct RenderPassColorAttachmentDescriptorRef;
    type ParentType = RenderPassAttachmentDescriptorRef;
}

impl RenderPassColorAttachmentDescriptor {
    pub fn new() -> Self {
        unsafe {
            let class = class!(MTLRenderPassColorAttachmentDescriptor);
            msg_send![class, new]
        }
    }
}

impl RenderPassColorAttachmentDescriptorRef {
    pub fn clear_color(&self) -> MTLClearColor {
        unsafe { msg_send![self, clearColor] }
    }

    pub fn set_clear_color(&self, clear_color: MTLClearColor) {
        unsafe { msg_send![self, setClearColor: clear_color] }
    }
}

pub enum MTLRenderPassDepthAttachmentDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPassDepthAttachmentDescriptor;
    pub struct RenderPassDepthAttachmentDescriptor;
    pub struct RenderPassDepthAttachmentDescriptorRef;
    type ParentType = RenderPassAttachmentDescriptorRef;
}

impl RenderPassDepthAttachmentDescriptorRef {
    pub fn clear_depth(&self) -> f64 {
        unsafe { msg_send![self, clearDepth] }
    }

    pub fn set_clear_depth(&self, clear_depth: f64) {
        unsafe { msg_send![self, setClearDepth: clear_depth] }
    }
}

pub enum MTLRenderPassStencilAttachmentDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPassStencilAttachmentDescriptor;
    pub struct RenderPassStencilAttachmentDescriptor;
    pub struct RenderPassStencilAttachmentDescriptorRef;
    type ParentType = RenderPassAttachmentDescriptorRef;
}

impl RenderPassStencilAttachmentDescriptorRef {
    pub fn clear_stencil(&self) -> u32 {
        unsafe { msg_send![self, clearStencil] }
    }

    pub fn set_clear_stencil(&self, clear_stencil: u32) {
        unsafe { msg_send![self, setClearStencil: clear_stencil] }
    }

    pub fn stencil_resolve_filter(&self) -> MTLMultisampleStencilResolveFilter {
        unsafe { msg_send![self, stencilResolveFilter] }
    }

    pub fn set_stencil_resolve_filter(
        &self,
        stencil_resolve_filter: MTLMultisampleStencilResolveFilter,
    ) {
        unsafe { msg_send![self, setStencilResolveFilter: stencil_resolve_filter] }
    }
}

pub enum MTLRenderPassColorAttachmentDescriptorArray {}

foreign_obj_type! {
    type CType = MTLRenderPassColorAttachmentDescriptorArray;
    pub struct RenderPassColorAttachmentDescriptorArray;
    pub struct RenderPassColorAttachmentDescriptorArrayRef;
}

impl RenderPassColorAttachmentDescriptorArrayRef {
    pub fn object_at(&self, index: NSUInteger) -> Option<&RenderPassColorAttachmentDescriptorRef> {
        unsafe { msg_send![self, objectAtIndexedSubscript: index] }
    }

    pub fn set_object_at(
        &self,
        index: NSUInteger,
        attachment: Option<&RenderPassColorAttachmentDescriptorRef>,
    ) {
        unsafe {
            msg_send![self, setObject:attachment
                     atIndexedSubscript:index]
        }
    }
}

pub enum MTLRenderPassDescriptor {}

foreign_obj_type! {
    type CType = MTLRenderPassDescriptor;
    pub struct RenderPassDescriptor;
    pub struct RenderPassDescriptorRef;
}

impl RenderPassDescriptor {
    pub fn new<'a>() -> &'a RenderPassDescriptorRef {
        unsafe {
            let class = class!(MTLRenderPassDescriptorInternal);
            msg_send![class, renderPassDescriptor]
        }
    }
}

impl RenderPassDescriptorRef {
    pub fn color_attachments(&self) -> &RenderPassColorAttachmentDescriptorArrayRef {
        unsafe { msg_send![self, colorAttachments] }
    }

    pub fn depth_attachment(&self) -> Option<&RenderPassDepthAttachmentDescriptorRef> {
        unsafe { msg_send![self, depthAttachment] }
    }

    pub fn set_depth_attachment(
        &self,
        depth_attachment: Option<&RenderPassDepthAttachmentDescriptorRef>,
    ) {
        unsafe { msg_send![self, setDepthAttachment: depth_attachment] }
    }

    pub fn stencil_attachment(&self) -> Option<&RenderPassStencilAttachmentDescriptorRef> {
        unsafe { msg_send![self, stencilAttachment] }
    }

    pub fn set_stencil_attachment(
        &self,
        stencil_attachment: Option<&RenderPassStencilAttachmentDescriptorRef>,
    ) {
        unsafe { msg_send![self, setStencilAttachment: stencil_attachment] }
    }

    pub fn visibility_result_buffer(&self) -> Option<&BufferRef> {
        unsafe { msg_send![self, visibilityResultBuffer] }
    }

    pub fn set_visibility_result_buffer(&self, buffer: Option<&BufferRef>) {
        unsafe { msg_send![self, setVisibilityResultBuffer: buffer] }
    }

    pub fn render_target_array_length(&self) -> NSUInteger {
        unsafe { msg_send![self, renderTargetArrayLength] }
    }

    pub fn set_render_target_array_length(&self, length: NSUInteger) {
        unsafe { msg_send![self, setRenderTargetArrayLength: length] }
    }

    pub fn render_target_width(&self) -> NSUInteger {
        unsafe { msg_send![self, renderTargetWidth] }
    }

    pub fn set_render_target_width(&self, size: NSUInteger) {
        unsafe { msg_send![self, setRenderTargetWidth: size] }
    }

    pub fn render_target_height(&self) -> NSUInteger {
        unsafe { msg_send![self, renderTargetHeight] }
    }

    pub fn set_render_target_height(&self, size: NSUInteger) {
        unsafe { msg_send![self, setRenderTargetHeight: size] }
    }

    pub fn default_raster_sample_count(&self) -> NSUInteger {
        unsafe { msg_send![self, defaultRasterSampleCount] }
    }

    pub fn set_default_raster_sample_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setDefaultRasterSampleCount: count] }
    }
}
