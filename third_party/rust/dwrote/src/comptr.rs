/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ops::{Deref, DerefMut};
use std::ptr;
use winapi::{IUnknown, REFIID, S_OK, E_NOINTERFACE};

#[derive(Debug)]
pub struct ComPtr<T> {
    ptr: *mut T,
}

impl<T> ComPtr<T> {
    pub fn new() -> Self {
        ComPtr { ptr: ptr::null_mut() }
    }

    pub fn from_ptr(ptr: *mut T) -> Self {
        unsafe {
            if !ptr.is_null() {
                (*(ptr as *mut IUnknown)).AddRef();
            }
        }
        ComPtr { ptr: ptr }
    }

    pub fn already_addrefed(ptr: *mut T) -> Self {
        ComPtr { ptr: ptr }
    }

    pub fn getter_addrefs<Q>(&mut self) -> *mut *mut Q {
        self.release();
        return &mut self.ptr as *mut *mut _ as *mut *mut Q;
    }

    pub fn as_ptr(&self) -> *mut T {
        self.ptr
    }

    pub fn query_interface<Q>(&self, iid: REFIID) -> Option<ComPtr<Q>> {
        if self.ptr.is_null() {
            return None;
        }

        unsafe {
            let mut p = ComPtr::<Q>::new();
            let hr = (*(self.ptr as *mut IUnknown)).QueryInterface(iid, p.getter_addrefs());
            if hr == S_OK {
                return Some(p);
            }
            assert!(hr == E_NOINTERFACE);
            return None;
        }
    }

    pub fn addref(&self) {
        unsafe {
            assert!(!self.ptr.is_null());
            (*(self.ptr as *mut IUnknown)).AddRef();
        }
    }

    pub fn release(&self) {
        unsafe {
            if !self.ptr.is_null() {
                (*(self.ptr as *mut IUnknown)).Release();
            }
        }
    }

    pub fn forget(&mut self) -> *mut T {
        let ptr = self.ptr;
        self.ptr = ptr::null_mut();
        ptr
    }
}

impl<T> Clone for ComPtr<T> {
    fn clone(&self) -> Self {
        if !self.ptr.is_null() {
            self.addref();
        }
        ComPtr { ptr: self.ptr }
    }
}

impl<T> Deref for ComPtr<T> {
    type Target = T;
    fn deref(&self) -> &T {
        assert!(!self.ptr.is_null());
        unsafe { &mut *self.ptr }
    }
}

impl<T> DerefMut for ComPtr<T> {
    fn deref_mut(&mut self) -> &mut T {
        assert!(!self.ptr.is_null());
        unsafe { &mut *self.ptr }
    }
}

impl<T> PartialEq for ComPtr<T> {
    fn eq(&self, other: &ComPtr<T>) -> bool {
        self.ptr == other.ptr
    }
}

impl<T> Drop for ComPtr<T> {
    fn drop(&mut self) {
        self.release();
    }
}

unsafe impl<T> Send for ComPtr<T> {}
