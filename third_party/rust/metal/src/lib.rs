// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]

#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate log;
#[macro_use]
extern crate objc;
#[macro_use]
extern crate foreign_types;

use std::{
    borrow::{Borrow, ToOwned},
    marker::PhantomData,
    mem,
    ops::Deref,
    os::raw::c_void,
};

use core_graphics_types::{base::CGFloat, geometry::CGSize};
use foreign_types::ForeignType;
use objc::runtime::{Object, NO, YES};

#[cfg(target_pointer_width = "64")]
pub type NSInteger = i64;
#[cfg(not(target_pointer_width = "64"))]
pub type NSInteger = i32;
#[cfg(target_pointer_width = "64")]
pub type NSUInteger = u64;
#[cfg(target_pointer_width = "32")]
pub type NSUInteger = u32;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct NSRange {
    pub location: NSUInteger,
    pub length: NSUInteger,
}

impl NSRange {
    #[inline]
    pub fn new(location: NSUInteger, length: NSUInteger) -> NSRange {
        NSRange { location, length }
    }
}

fn nsstring_as_str(nsstr: &objc::runtime::Object) -> &str {
    let bytes = unsafe {
        let bytes: *const std::os::raw::c_char = msg_send![nsstr, UTF8String];
        bytes as *const u8
    };
    let len: NSUInteger = unsafe { msg_send![nsstr, length] };
    unsafe {
        let bytes = std::slice::from_raw_parts(bytes, len as usize);
        std::str::from_utf8(bytes).unwrap()
    }
}

fn nsstring_from_str(string: &str) -> *mut objc::runtime::Object {
    const UTF8_ENCODING: usize = 4;

    let cls = class!(NSString);
    let bytes = string.as_ptr() as *const c_void;
    unsafe {
        let obj: *mut objc::runtime::Object = msg_send![cls, alloc];
        let obj: *mut objc::runtime::Object = msg_send![
            obj,
            initWithBytes:bytes
            length:string.len()
            encoding:UTF8_ENCODING
        ];
        let _: *mut c_void = msg_send![obj, autorelease];
        obj
    }
}

macro_rules! foreign_obj_type {
    {type CType = $raw_ident:ident;
    pub struct $owned_ident:ident;
    pub struct $ref_ident:ident;
    type ParentType = $parent_ref:ident;
    } => {
        foreign_obj_type! {
            type CType = $raw_ident;
            pub struct $owned_ident;
            pub struct $ref_ident;
        }

        impl ::std::ops::Deref for $ref_ident {
            type Target = $parent_ref;

            fn deref(&self) -> &$parent_ref {
                unsafe { &*(self as *const $ref_ident as *const $parent_ref)  }
            }
        }
    };
    {type CType = $raw_ident:ident;
    pub struct $owned_ident:ident;
    pub struct $ref_ident:ident;
    } => {
        foreign_type! {
            type CType = $raw_ident;
            fn drop = crate::obj_drop;
            fn clone = crate::obj_clone;
            pub struct $owned_ident;
            pub struct $ref_ident;
        }

        unsafe impl ::objc::Message for $raw_ident {
        }
        unsafe impl ::objc::Message for $ref_ident {
        }

        impl ::std::fmt::Debug for $ref_ident {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                unsafe {
                    let string: *mut ::objc::runtime::Object = msg_send![self, debugDescription];
                    write!(f, "{}", crate::nsstring_as_str(&*string))
                }
            }
        }

        impl ::std::fmt::Debug for $owned_ident {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                ::std::ops::Deref::deref(self).fmt(f)
            }
        }
    };
}

macro_rules! try_objc {
    {
        $err_name: ident => $body:expr
    } => {
        {
            let mut $err_name: *mut ::objc::runtime::Object = ::std::ptr::null_mut();
            let value = $body;
            if !$err_name.is_null() {
                let desc: *mut Object = msg_send![$err_name, localizedDescription];
                let compile_error: *const std::os::raw::c_char = msg_send![desc, UTF8String];
                let message = CStr::from_ptr(compile_error).to_string_lossy().into_owned();
                let () = msg_send![$err_name, release];
                return Err(message);
            }
            value
        }
    };
}

pub struct NSArray<T> {
    _phantom: PhantomData<T>,
}

pub struct Array<T>(*mut NSArray<T>)
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static;
pub struct ArrayRef<T>(foreign_types::Opaque, PhantomData<T>)
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static;

impl<T> Drop for Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    fn drop(&mut self) {
        unsafe {
            let () = msg_send![self.0, release];
        }
    }
}

impl<T> Clone for Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    fn clone(&self) -> Self {
        unsafe { Array(msg_send![self.0, retain]) }
    }
}

unsafe impl<T> objc::Message for NSArray<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
}
unsafe impl<T> objc::Message for ArrayRef<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
}

impl<T> Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    pub fn from_slice<'a>(s: &[&T::Ref]) -> &'a ArrayRef<T> {
        unsafe {
            let class = class!(NSArray);
            msg_send![class, arrayWithObjects: s.as_ptr() count: s.len()]
        }
    }

    pub fn from_owned_slice<'a>(s: &[T]) -> &'a ArrayRef<T> {
        unsafe {
            let class = class!(NSArray);
            msg_send![class, arrayWithObjects: s.as_ptr() count: s.len()]
        }
    }
}

impl<T> foreign_types::ForeignType for Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    type CType = NSArray<T>;
    type Ref = ArrayRef<T>;

    unsafe fn from_ptr(p: *mut NSArray<T>) -> Self {
        Array(p)
    }

    fn as_ptr(&self) -> *mut NSArray<T> {
        self.0
    }
}

impl<T> foreign_types::ForeignTypeRef for ArrayRef<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    type CType = NSArray<T>;
}

impl<T> Deref for Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    type Target = ArrayRef<T>;

    fn deref(&self) -> &ArrayRef<T> {
        unsafe { mem::transmute(self.as_ptr()) }
    }
}

impl<T> Borrow<ArrayRef<T>> for Array<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    fn borrow(&self) -> &ArrayRef<T> {
        unsafe { mem::transmute(self.as_ptr()) }
    }
}

impl<T> ToOwned for ArrayRef<T>
where
    T: ForeignType + 'static,
    T::Ref: objc::Message + 'static,
{
    type Owned = Array<T>;

    fn to_owned(&self) -> Array<T> {
        unsafe { Array::from_ptr(msg_send![self, retain]) }
    }
}

pub enum CAMetalDrawable {}

foreign_obj_type! {
    type CType = CAMetalDrawable;
    pub struct MetalDrawable;
    pub struct MetalDrawableRef;
    type ParentType = DrawableRef;
}

impl MetalDrawableRef {
    pub fn texture(&self) -> &TextureRef {
        unsafe { msg_send![self, texture] }
    }
}

pub enum CAMetalLayer {}

foreign_obj_type! {
    type CType = CAMetalLayer;
    pub struct MetalLayer;
    pub struct MetalLayerRef;
}

impl MetalLayer {
    pub fn new() -> Self {
        unsafe {
            let class = class!(CAMetalLayer);
            msg_send![class, new]
        }
    }
}

impl MetalLayerRef {
    pub fn device(&self) -> &DeviceRef {
        unsafe { msg_send![self, device] }
    }

    pub fn set_device(&self, device: &DeviceRef) {
        unsafe { msg_send![self, setDevice: device] }
    }

    pub fn pixel_format(&self) -> MTLPixelFormat {
        unsafe { msg_send![self, pixelFormat] }
    }

    pub fn set_pixel_format(&self, pixel_format: MTLPixelFormat) {
        unsafe { msg_send![self, setPixelFormat: pixel_format] }
    }

    pub fn drawable_size(&self) -> CGSize {
        unsafe { msg_send![self, drawableSize] }
    }

    pub fn set_drawable_size(&self, size: CGSize) {
        unsafe { msg_send![self, setDrawableSize: size] }
    }

    pub fn presents_with_transaction(&self) -> bool {
        unsafe {
            match msg_send![self, presentsWithTransaction] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_presents_with_transaction(&self, transaction: bool) {
        unsafe { msg_send![self, setPresentsWithTransaction: transaction] }
    }

    pub fn display_sync_enabled(&self) -> bool {
        unsafe {
            match msg_send![self, displaySyncEnabled] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_display_sync_enabled(&self, enabled: bool) {
        unsafe { msg_send![self, setDisplaySyncEnabled: enabled] }
    }

    pub fn maximum_drawable_count(&self) -> NSUInteger {
        unsafe { msg_send![self, maximumDrawableCount] }
    }

    pub fn set_maximum_drawable_count(&self, count: NSUInteger) {
        unsafe { msg_send![self, setMaximumDrawableCount: count] }
    }

    pub fn set_edge_antialiasing_mask(&self, mask: u64) {
        unsafe { msg_send![self, setEdgeAntialiasingMask: mask] }
    }

    pub fn set_masks_to_bounds(&self, masks: bool) {
        unsafe { msg_send![self, setMasksToBounds: masks] }
    }

    pub fn remove_all_animations(&self) {
        unsafe { msg_send![self, removeAllAnimations] }
    }

    pub fn next_drawable(&self) -> Option<&MetalDrawableRef> {
        unsafe { msg_send![self, nextDrawable] }
    }

    pub fn set_contents_scale(&self, scale: CGFloat) {
        unsafe { msg_send![self, setContentsScale: scale] }
    }

    /// [framebufferOnly Apple Docs](https://developer.apple.com/documentation/metal/mtltexture/1515749-framebufferonly?language=objc)
    pub fn framebuffer_only(&self) -> bool {
        unsafe {
            match msg_send![self, framebufferOnly] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_framebuffer_only(&self, framebuffer_only: bool) {
        unsafe { msg_send![self, setFramebufferOnly: framebuffer_only] }
    }

    pub fn is_opaque(&self) -> bool {
        unsafe {
            match msg_send![self, isOpaque] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_opaque(&self, opaque: bool) {
        unsafe { msg_send![self, setOpaque: opaque] }
    }

    pub fn wants_extended_dynamic_range_content(&self) -> bool {
        unsafe {
            match msg_send![self, wantsExtendedDynamicRangeContent] {
                YES => true,
                NO => false,
                _ => unreachable!(),
            }
        }
    }

    pub fn set_wants_extended_dynamic_range_content(
        &self,
        wants_extended_dynamic_range_content: bool,
    ) {
        unsafe {
            msg_send![
                self,
                setWantsExtendedDynamicRangeContent: wants_extended_dynamic_range_content
            ]
        }
    }
}

mod argument;
mod buffer;
mod capturedescriptor;
mod capturemanager;
mod commandbuffer;
mod commandqueue;
mod constants;
mod depthstencil;
mod device;
mod drawable;
mod encoder;
mod heap;
mod indirect_encoder;
mod library;
#[cfg(feature = "mps")]
mod mps;
mod pipeline;
mod renderpass;
mod resource;
mod sampler;
mod sync;
mod texture;
mod types;
mod vertexdescriptor;

#[rustfmt::skip]
pub use {
    argument::*,
    buffer::*,
    capturedescriptor::*,
    capturemanager::*,
    commandbuffer::*,
    commandqueue::*,
    constants::*,
    depthstencil::*,
    device::*,
    drawable::*,
    encoder::*,
    heap::*,
    indirect_encoder::*,
    library::*,
    pipeline::*,
    renderpass::*,
    resource::*,
    sampler::*,
    texture::*,
    types::*,
    vertexdescriptor::*,
    sync::*,
};

#[cfg(feature = "mps")]
pub use mps::*;

#[inline]
unsafe fn obj_drop<T>(p: *mut T) {
    msg_send![(p as *mut Object), release]
}

#[inline]
unsafe fn obj_clone<T: 'static>(p: *mut T) -> *mut T {
    msg_send![(p as *mut Object), retain]
}

#[allow(non_camel_case_types)]
type c_size_t = usize;

// TODO: expand supported interface
pub enum NSURL {}

foreign_obj_type! {
    type CType = NSURL;
    pub struct URL;
    pub struct URLRef;
}

impl URL {
    pub fn new_with_string(string: &str) -> Self {
        unsafe {
            let ns_str = crate::nsstring_from_str(string);
            let class = class!(NSURL);
            msg_send![class, URLWithString: ns_str]
        }
    }
}

impl URLRef {
    pub fn absolute_string(&self) -> &str {
        unsafe {
            let absolute_string = msg_send![self, absoluteString];
            crate::nsstring_as_str(absolute_string)
        }
    }
}
