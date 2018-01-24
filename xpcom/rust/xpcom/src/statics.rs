/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::ffi::CStr;
use std::ptr;
use nserror::NsresultExt;
use {
    RefPtr,
    GetterAddrefs,
    XpCom,
};

use interfaces::{
    nsIComponentManager,
    nsIServiceManager,
    nsIComponentRegistrar,
};

macro_rules! try_opt {
    ($e: expr) => {
        match $e {
            Some(x) => x,
            None => return None,
        }
    }
}

/// Get a reference to the global `nsIComponentManager`.
///
/// Can return `None` during shutdown.
#[inline]
pub fn component_manager() -> Option<RefPtr<nsIComponentManager>> {
    unsafe {
        RefPtr::from_raw(Gecko_GetComponentManager())
    }
}

/// Get a reference to the global `nsIServiceManager`.
///
/// Can return `None` during shutdown.
#[inline]
pub fn service_manager() -> Option<RefPtr<nsIServiceManager>> {
    unsafe {
        RefPtr::from_raw(Gecko_GetServiceManager())
    }
}

/// Get a reference to the global `nsIComponentRegistrar`
///
/// Can return `None` during shutdown.
#[inline]
pub fn component_registrar() -> Option<RefPtr<nsIComponentRegistrar>> {
    unsafe {
        RefPtr::from_raw(Gecko_GetComponentRegistrar())
    }
}

/// Helper for calling `nsIComponentManager::CreateInstanceByContractID` on the
/// global `nsIComponentRegistrar`.
///
/// This method is similar to `do_CreateInstance` in C++.
#[inline]
pub fn create_instance<T: XpCom>(id: &CStr) -> Option<RefPtr<T>> {
    unsafe {
        let mut ga = GetterAddrefs::<T>::new();
        if try_opt!(component_manager()).CreateInstanceByContractID(
            id.as_ptr(),
            ptr::null(),
            &T::IID,
            ga.void_ptr(),
        ).succeeded() {
            ga.refptr()
        } else {
            None
        }
    }
}

/// Helper for calling `nsIServiceManager::GetServiceByContractID` on the global
/// `nsIServiceManager`.
///
/// This method is similar to `do_GetService` in C++.
#[inline]
pub fn get_service<T: XpCom>(id: &CStr) -> Option<RefPtr<T>> {
    unsafe {
        let mut ga = GetterAddrefs::<T>::new();
        if try_opt!(service_manager()).GetServiceByContractID(
            id.as_ptr(),
            &T::IID,
            ga.void_ptr()
        ).succeeded() {
            ga.refptr()
        } else {
            None
        }
    }
}

extern "C" {
    fn Gecko_GetComponentManager() -> *const nsIComponentManager;
    fn Gecko_GetServiceManager() -> *const nsIServiceManager;
    fn Gecko_GetComponentRegistrar() -> *const nsIComponentRegistrar;
}
