/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use crate::{
    create_instance,
    interfaces::{nsIVariant, nsIWritableVariant},
    RefCounted,
};

use cstr::*;

mod ffi {
    use super::*;

    extern "C" {
        // These are implemented in dom/promise/Promise.cpp
        pub fn DomPromise_AddRef(promise: *const Promise);
        pub fn DomPromise_Release(promise: *const Promise);
        pub fn DomPromise_RejectWithVariant(promise: *const Promise, variant: *const nsIVariant);
        pub fn DomPromise_ResolveWithVariant(promise: *const Promise, variant: *const nsIVariant);
    }
}

#[repr(C)]
pub struct Promise {
    private: [u8; 0],

    /// This field is a phantomdata to ensure that the Promise type and any
    /// struct containing it is not safe to send across threads, as DOM is
    /// generally not threadsafe.
    __nosync: ::std::marker::PhantomData<::std::rc::Rc<u8>>,
}

impl Promise {
    pub fn reject_with_undefined(&self) {
        let variant = create_instance::<nsIWritableVariant>(cstr!("@mozilla.org/variant;1"))
            .expect("Failed to create writable variant");
        unsafe {
            variant.SetAsVoid();
        }
        self.reject_with_variant(&variant);
    }

    pub fn reject_with_variant(&self, variant: &nsIVariant) {
        unsafe { ffi::DomPromise_RejectWithVariant(self, variant) }
    }

    pub fn resolve_with_variant(&self, variant: &nsIVariant) {
        unsafe { ffi::DomPromise_ResolveWithVariant(self, variant) }
    }
}

unsafe impl RefCounted for Promise {
    unsafe fn addref(&self) {
        ffi::DomPromise_AddRef(self)
    }

    unsafe fn release(&self) {
        ffi::DomPromise_Release(self)
    }
}
