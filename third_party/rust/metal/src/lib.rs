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

use std::borrow::{Borrow, ToOwned};
use std::marker::PhantomData;
use std::mem;
use std::ops::Deref;
use std::os::raw::c_void;

use core_graphics::base::CGFloat;
use core_graphics::geometry::CGSize;
use foreign_types::ForeignType;
use objc::runtime::{Object, NO, YES};
use cocoa::foundation::NSUInteger;

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
    pub struct CoreAnimationDrawable;
    pub struct CoreAnimationDrawableRef;
    type ParentType = DrawableRef;
}

impl CoreAnimationDrawableRef {
    pub fn texture(&self) -> &TextureRef {
        unsafe { msg_send![self, texture] }
    }
}

pub enum CAMetalLayer {}

foreign_obj_type! {
    type CType = CAMetalLayer;
    pub struct CoreAnimationLayer;
    pub struct CoreAnimationLayerRef;
}

impl CoreAnimationLayer {
    pub fn new() -> Self {
        unsafe {
            let class = class!(CAMetalLayer);
            msg_send![class, new]
        }
    }
}

impl CoreAnimationLayerRef {
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

    pub fn set_edge_antialiasing_mask(&self, mask: u64) {
        unsafe { msg_send![self, setEdgeAntialiasingMask: mask] }
    }

    pub fn set_masks_to_bounds(&self, masks: bool) {
        unsafe { msg_send![self, setMasksToBounds: masks] }
    }

    pub fn remove_all_animations(&self) {
        unsafe { msg_send![self, removeAllAnimations] }
    }

    pub fn next_drawable(&self) -> Option<&CoreAnimationDrawableRef> {
        unsafe { msg_send![self, nextDrawable] }
    }

    pub fn set_contents_scale(&self, scale: CGFloat) {
        unsafe { msg_send![self, setContentsScale: scale] }
    }
}

mod argument;
mod buffer;
mod capturemanager;
mod commandbuffer;
mod commandqueue;
mod constants;
mod depthstencil;
mod device;
mod drawable;
mod encoder;
mod heap;
mod library;
mod pipeline;
mod renderpass;
mod resource;
mod sampler;
mod texture;
mod types;
mod vertexdescriptor;

pub use argument::*;
pub use buffer::*;
pub use capturemanager::*;
pub use commandbuffer::*;
pub use commandqueue::*;
pub use constants::*;
pub use depthstencil::*;
pub use device::*;
pub use drawable::*;
pub use encoder::*;
pub use heap::*;
pub use library::*;
pub use pipeline::*;
pub use renderpass::*;
pub use resource::*;
pub use sampler::*;
pub use texture::*;
pub use types::*;
pub use vertexdescriptor::*;

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
