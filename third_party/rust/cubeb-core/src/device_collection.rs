// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use ffi_types;
use std::{ops, slice};
use {ContextRef, DeviceInfo};

/// A collection of `DeviceInfo` used by libcubeb
type CType = ffi::cubeb_device_collection;

#[derive(Debug)]
pub struct DeviceCollection<'ctx>(CType, &'ctx ContextRef);

impl<'ctx> DeviceCollection<'ctx> {
    pub(crate) fn init_with_ctx(ctx: &ContextRef, coll: CType) -> DeviceCollection {
        DeviceCollection(coll, ctx)
    }

    pub fn as_ptr(&self) -> *mut CType {
        &self.0 as *const CType as *mut CType
    }
}

impl<'ctx> Drop for DeviceCollection<'ctx> {
    fn drop(&mut self) {
        unsafe {
            let _ = call!(ffi::cubeb_device_collection_destroy(
                self.1.as_ptr(),
                &mut self.0
            ));
        }
    }
}

impl<'ctx> ::std::ops::Deref for DeviceCollection<'ctx> {
    type Target = DeviceCollectionRef;

    #[inline]
    fn deref(&self) -> &DeviceCollectionRef {
        let ptr = &self.0 as *const CType as *mut CType;
        unsafe { DeviceCollectionRef::from_ptr(ptr) }
    }
}

impl<'ctx> ::std::convert::AsRef<DeviceCollectionRef> for DeviceCollection<'ctx> {
    #[inline]
    fn as_ref(&self) -> &DeviceCollectionRef {
        self
    }
}

pub struct DeviceCollectionRef(ffi_types::Opaque);

impl DeviceCollectionRef {
    /// # Safety
    ///
    /// This function is unsafe because it dereferences the given `ptr` pointer.
    /// The caller should ensure that pointer is valid.
    #[inline]
    pub unsafe fn from_ptr<'a>(ptr: *mut CType) -> &'a Self {
        &*(ptr as *mut _)
    }

    /// # Safety
    ///
    /// This function is unsafe because it dereferences the given `ptr` pointer.
    /// The caller should ensure that pointer is valid.
    #[inline]
    pub unsafe fn from_ptr_mut<'a>(ptr: *mut CType) -> &'a mut Self {
        &mut *(ptr as *mut _)
    }

    #[inline]
    pub fn as_ptr(&self) -> *mut CType {
        self as *const _ as *mut _
    }
}

impl ::std::fmt::Debug for DeviceCollectionRef {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        let ptr = self as *const DeviceCollectionRef as usize;
        f.debug_tuple(stringify!(DeviceCollectionRef))
            .field(&ptr)
            .finish()
    }
}

impl ops::Deref for DeviceCollectionRef {
    type Target = [DeviceInfo];
    fn deref(&self) -> &[DeviceInfo] {
        unsafe {
            let coll: &ffi::cubeb_device_collection = &*self.as_ptr();
            slice::from_raw_parts(coll.device as *const DeviceInfo, coll.count)
        }
    }
}
